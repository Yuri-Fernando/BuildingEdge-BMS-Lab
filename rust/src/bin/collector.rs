use buildingedge_bms::{ConnectionPermit, Frame, Store, FRAME_SIZE};
use std::io::{Read, Write};
use std::net::{TcpListener, TcpStream};
use std::sync::{atomic::AtomicUsize, Arc, Mutex};
use std::thread;
use std::time::{Duration, Instant};

type Shared = Arc<Mutex<Store>>;
const STALE_MS: u64 = 3000;
const INGEST_LIMIT: usize = 64;
const HTTP_LIMIT: usize = 32;
const INDEX: &str = include_str!("../../../web/index.html");
const CSS: &str = include_str!("../../../web/app.css");
const JS: &str = include_str!("../../../web/app.js");

fn argument(args: &[String], key: &str, fallback: &str) -> String {
    args.windows(2)
        .find(|w| w[0] == key)
        .map(|w| w[1].clone())
        .unwrap_or_else(|| fallback.to_owned())
}
fn client(mut stream: TcpStream, store: Shared) {
    let _ = stream.set_read_timeout(Some(Duration::from_secs(3)));
    loop {
        let mut bytes = [0u8; FRAME_SIZE];
        if stream.read_exact(&mut bytes).is_err() {
            break;
        }
        let mut state = match store.lock() {
            Ok(s) => s,
            Err(_) => break,
        };
        match Frame::parse(&bytes) {
            Ok(frame) => {
                let _ = state.accept(frame);
            }
            Err(_) => state.reject(),
        }
    }
}
fn frame_json(s: &Frame) -> String {
    format!("{{\"id\":{},\"seq\":\"{}\",\"ts_ms\":{},\"temp_c\":{:.1},\"co2_ppm\":{},\"rh_pct\":{:.1},\"occupied\":{},\"hvac_on\":{},\"co2_alarm\":{},\"watts\":{}}}",
        s.device_id, s.seq, s.timestamp_ms, s.temp_deci_c as f64 / 10.0,
        s.co2_ppm, s.rh_deci_pct as f64 / 10.0, s.occupied, s.hvac_on, s.alarm, s.watts)
}
fn latest(state: &Store) -> String {
    let devices: Vec<_> = state
        .last
        .values()
        .map(|s| {
            let mut json = frame_json(s);
            json.pop();
            let age = state.age_ms(s.device_id).unwrap_or(u64::MAX);
            json.push_str(&format!(
                ",\"age_ms\":{},\"stale\":{}}}",
                age,
                age > STALE_MS
            ));
            json
        })
        .collect();
    format!("{{\"devices\":[{}],\"accepted\":{},\"rejected\":{},\"evicted\":{},\"history_size\":{},\"capacity\":{},\"stale_after_ms\":{}}}",
        devices.join(","), state.accepted, state.rejected, state.evicted,
        state.history_len(), state.capacity(), STALE_MS)
}
fn history(state: &Store) -> String {
    let samples: Vec<_> = state.history().map(frame_json).collect();
    format!(
        "{{\"samples\":[{}],\"capacity\":{}}}",
        samples.join(","),
        state.capacity()
    )
}
fn metrics(s: &Store) -> String {
    let mut out = format!(
        "# HELP bms_samples_accepted_total Valid telemetry frames accepted.\n# TYPE bms_samples_accepted_total counter\nbms_samples_accepted_total {}\n\
# HELP bms_samples_rejected_total Invalid or replayed frames rejected.\n# TYPE bms_samples_rejected_total counter\nbms_samples_rejected_total {}\n\
# HELP bms_ring_evictions_total Old samples evicted from bounded history.\n# TYPE bms_ring_evictions_total counter\nbms_ring_evictions_total {}\n\
# HELP bms_devices Known devices in the collector.\n# TYPE bms_devices gauge\nbms_devices {}\n\
# HELP bms_ring_size Retained samples in bounded history.\n# TYPE bms_ring_size gauge\nbms_ring_size {}\n\
# HELP bms_device_sample_age_seconds Monotonic time since last accepted sample.\n# TYPE bms_device_sample_age_seconds gauge\n",
        s.accepted, s.rejected, s.evicted, s.last.len(), s.history_len());
    for id in s.last.keys() {
        out.push_str(&format!(
            "bms_device_sample_age_seconds{{device=\"{}\"}} {:.3}\n",
            id,
            s.age_ms(*id).unwrap_or(u64::MAX) as f64 / 1000.0
        ));
    }
    out
}
fn read_request(stream: &mut TcpStream) -> std::io::Result<String> {
    let deadline = Instant::now() + Duration::from_secs(3);
    let mut buf = Vec::with_capacity(1024);
    while buf.len() < 8192 {
        let remaining = deadline.saturating_duration_since(Instant::now());
        if remaining.is_zero() {
            return Err(std::io::ErrorKind::TimedOut.into());
        }
        stream.set_read_timeout(Some(remaining))?;
        let mut chunk = [0u8; 512];
        let n = stream.read(&mut chunk)?;
        if n == 0 {
            return Err(std::io::ErrorKind::UnexpectedEof.into());
        }
        buf.extend_from_slice(&chunk[..n]);
        if buf.windows(4).any(|v| v == b"\r\n\r\n") {
            return String::from_utf8(buf).map_err(|_| std::io::ErrorKind::InvalidData.into());
        }
    }
    Err(std::io::ErrorKind::InvalidData.into())
}
fn http_client(mut stream: TcpStream, store: Shared) {
    let _ = stream.set_write_timeout(Some(Duration::from_secs(3)));
    let req = match read_request(&mut stream) {
        Ok(r) => r,
        Err(_) => return,
    };
    let first: Vec<_> = req
        .lines()
        .next()
        .unwrap_or("")
        .split_whitespace()
        .collect();
    let (code, content_type, body) =
        if first.len() != 3 || !matches!(first[2], "HTTP/1.1" | "HTTP/1.0") {
            (400, "text/plain", "bad request\n".to_string())
        } else if first[0] != "GET" {
            (405, "text/plain", "method not allowed\n".to_string())
        } else {
            match first[1] {
                "/" => (200, "text/html; charset=utf-8", INDEX.to_string()),
                "/app.css" => (200, "text/css; charset=utf-8", CSS.to_string()),
                "/app.js" => (200, "text/javascript; charset=utf-8", JS.to_string()),
                "/healthz" => (200, "application/json", "{\"status\":\"ok\"}".to_string()),
                "/v1/latest" | "/v1/history" | "/metrics" => {
                    let state = match store.lock() {
                        Ok(s) => s,
                        Err(_) => return,
                    };
                    match first[1] {
                        "/v1/latest" => (200, "application/json", latest(&state)),
                        "/v1/history" => (200, "application/json", history(&state)),
                        _ => (200, "text/plain; version=0.0.4", metrics(&state)),
                    }
                }
                _ => (404, "text/plain", "not found\n".to_string()),
            }
        };
    let reason = match code {
        200 => "OK",
        400 => "Bad Request",
        405 => "Method Not Allowed",
        _ => "Not Found",
    };
    let header = format!("HTTP/1.1 {} {}\r\nContent-Type: {}\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nContent-Security-Policy: default-src 'self'; style-src 'self'; script-src 'self'; connect-src 'self'; img-src 'self' data:; object-src 'none'; base-uri 'none'; frame-ancestors 'none'\r\nContent-Length: {}\r\nConnection: close\r\n{}\r\n",
        code, reason, content_type, body.len(), if code == 405 { "Allow: GET\r\n" } else { "" });
    let _ = stream.write_all(header.as_bytes());
    let _ = stream.write_all(body.as_bytes());
}
fn main() -> std::io::Result<()> {
    let args: Vec<String> = std::env::args().collect();
    if args.iter().any(|a| a == "--help") {
        println!("bms-collector [--bind 127.0.0.1] [--ingest-port 17001] [--http-port 18080] [--capacity 512]");
        return Ok(());
    }
    let bind = argument(&args, "--bind", "127.0.0.1");
    let parse = |key: &str, default: &str| -> std::io::Result<usize> {
        argument(&args, key, default).parse().map_err(|_| {
            std::io::Error::new(std::io::ErrorKind::InvalidInput, format!("invalid {key}"))
        })
    };
    let ingest_port = parse("--ingest-port", "17001")?;
    let http_port = parse("--http-port", "18080")?;
    let capacity = parse("--capacity", "512")?;
    if !(1..=65535).contains(&ingest_port)
        || !(1..=65535).contains(&http_port)
        || !(1..=65536).contains(&capacity)
    {
        return Err(std::io::Error::new(
            std::io::ErrorKind::InvalidInput,
            "ports 1..65535 and capacity 1..65536 required",
        ));
    }
    let store: Shared = Arc::new(Mutex::new(Store::new(capacity)));
    let ingress = TcpListener::bind((bind.as_str(), ingest_port as u16))?;
    let http = TcpListener::bind((bind.as_str(), http_port as u16))?;
    let s = Arc::clone(&store);
    thread::spawn(move || {
        let active = Arc::new(AtomicUsize::new(0));
        for conn in ingress.incoming().flatten() {
            if let Some(permit) = ConnectionPermit::acquire(&active, INGEST_LIMIT) {
                let state = Arc::clone(&s);
                thread::spawn(move || {
                    let _permit = permit;
                    client(conn, state);
                });
            }
        }
    });
    eprintln!("bms-collector ingest {bind}:{ingest_port}, UI/API {bind}:{http_port}");
    let active = Arc::new(AtomicUsize::new(0));
    for conn in http.incoming().flatten() {
        if let Some(permit) = ConnectionPermit::acquire(&active, HTTP_LIMIT) {
            let state = Arc::clone(&store);
            thread::spawn(move || {
                let _permit = permit;
                http_client(conn, state);
            });
        }
    }
    Ok(())
}
