use buildingedge_bms::{parse_modbus_request, ConnectionPermit};
use std::io::{Read, Write};
use std::net::{TcpListener, TcpStream};
use std::sync::{
    atomic::{AtomicU64, AtomicUsize, Ordering},
    Arc,
};
use std::thread;
use std::time::Duration;

#[derive(Clone)]
struct Config {
    bind: String,
    port: u16,
    drop_every: u64,
    delay_ms: u64,
}
fn arg(args: &[String], key: &str, fallback: &str) -> String {
    args.windows(2)
        .find(|w| w[0] == key)
        .map(|w| w[1].clone())
        .unwrap_or_else(|| fallback.to_string())
}
fn device_registers(n: u64) -> [u16; 6] {
    let phase = n % 40;
    let co2 = if phase < 20 {
        850 + phase * 20
    } else {
        1250 - (phase - 20) * 20
    };
    [
        (230 + ((n % 10) as i16 - 5)) as u16,
        co2 as u16,
        450 + (n % 7) as u16,
        (n % 9 != 0) as u16,
        (co2 >= 950) as u16,
        (250 + n % 100) as u16,
    ]
}
fn run_client(mut stream: TcpStream, shared: Arc<AtomicU64>, cfg: Config) {
    let _ = stream.set_read_timeout(Some(Duration::from_secs(2)));
    let _ = stream.set_write_timeout(Some(Duration::from_secs(2)));
    loop {
        let mut header = [0u8; 7];
        if stream.read_exact(&mut header).is_err() {
            break;
        }
        let length = u16::from_be_bytes([header[4], header[5]]) as usize;
        if !(2..=253).contains(&length) {
            break;
        }
        let mut pdu = vec![0u8; length - 1];
        if stream.read_exact(&mut pdu).is_err() {
            break;
        }
        let (tid, unit) = match parse_modbus_request(&header, &pdu) {
            Ok(v) => v,
            Err(_) => break,
        };
        let n = shared.fetch_add(1, Ordering::Relaxed) + 1;
        if cfg.drop_every > 0 && n % cfg.drop_every == 0 {
            break;
        }
        if cfg.delay_ms > 0 {
            thread::sleep(Duration::from_millis(cfg.delay_ms));
        }
        let registers = device_registers(n);
        let mut reply = Vec::with_capacity(21);
        reply.extend_from_slice(&tid.to_be_bytes());
        reply.extend_from_slice(&0u16.to_be_bytes());
        reply.extend_from_slice(&15u16.to_be_bytes());
        reply.push(unit);
        reply.push(3);
        reply.push(12);
        for v in registers {
            reply.extend_from_slice(&v.to_be_bytes());
        }
        if stream.write_all(&reply).is_err() {
            break;
        }
    }
}
fn main() -> std::io::Result<()> {
    let args: Vec<String> = std::env::args().collect();
    if args.iter().any(|a| a == "--help") {
        println!("modbus-sim [--bind 127.0.0.1] [--port 15020] [--drop-every 0] [--delay-ms 0]");
        return Ok(());
    }
    let cfg = Config {
        bind: arg(&args, "--bind", "127.0.0.1"),
        port: arg(&args, "--port", "15020")
            .parse()
            .expect("--port must be u16"),
        drop_every: arg(&args, "--drop-every", "0")
            .parse()
            .expect("--drop-every must be u64"),
        delay_ms: arg(&args, "--delay-ms", "0")
            .parse()
            .expect("--delay-ms must be u64"),
    };
    let listener = TcpListener::bind((cfg.bind.as_str(), cfg.port))?;
    eprintln!(
        "modbus-sim listening on {}:{} (synthetic sensors only)",
        cfg.bind, cfg.port
    );
    let seq = Arc::new(AtomicU64::new(0));
    let active = Arc::new(AtomicUsize::new(0));
    for stream in listener.incoming() {
        match stream {
            Ok(stream) => {
                let seq = Arc::clone(&seq);
                let cfg = cfg.clone();
                if let Some(permit) = ConnectionPermit::acquire(&active, 64) {
                    thread::spawn(move || {
                        let _permit = permit;
                        run_client(stream, seq, cfg);
                    });
                }
            }
            Err(e) => eprintln!("accept error: {e}"),
        }
    }
    Ok(())
}
