//! BuildingEdge binary telemetry protocol and bounded in-memory collector.
//! No third-party dependencies. Not a standardized building-automation protocol.
use std::collections::{BTreeMap, VecDeque};

pub const FRAME_SIZE: usize = 31;
pub const MAX_DEVICES: usize = 128;

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Frame {
    pub device_id: u16,
    pub seq: u64,
    pub timestamp_ms: u64,
    pub temp_deci_c: i16,
    pub co2_ppm: u16,
    pub rh_deci_pct: u16,
    pub occupied: bool,
    pub hvac_on: bool,
    pub alarm: bool,
    pub watts: u16,
}

impl Frame {
    pub fn parse(bytes: &[u8]) -> Result<Self, &'static str> {
        if bytes.len() != FRAME_SIZE {
            return Err("wrong telemetry frame size");
        }
        if &bytes[0..4] != b"BMS1" {
            return Err("unknown telemetry protocol version");
        }
        let device_id = u16::from_be_bytes([bytes[4], bytes[5]]);
        let seq = u64::from_be_bytes(bytes[6..14].try_into().map_err(|_| "seq")?);
        let timestamp_ms = u64::from_be_bytes(bytes[14..22].try_into().map_err(|_| "timestamp")?);
        let temp_deci_c = i16::from_be_bytes([bytes[22], bytes[23]]);
        let co2_ppm = u16::from_be_bytes([bytes[24], bytes[25]]);
        let rh_deci_pct = u16::from_be_bytes([bytes[26], bytes[27]]);
        let flags = bytes[28];
        let watts = u16::from_be_bytes([bytes[29], bytes[30]]);
        if device_id == 0
            || seq == 0
            || !(-500..=1250).contains(&temp_deci_c)
            || co2_ppm > 10_000
            || rh_deci_pct > 1000
            || watts > 50_000
            || flags & !7 != 0
        {
            return Err("invalid telemetry values");
        }
        Ok(Self {
            device_id,
            seq,
            timestamp_ms,
            temp_deci_c,
            co2_ppm,
            rh_deci_pct,
            occupied: flags & 1 != 0,
            hvac_on: flags & 2 != 0,
            alarm: flags & 4 != 0,
            watts,
        })
    }
}

use std::sync::{
    atomic::{AtomicUsize, Ordering},
    Arc,
};
use std::time::Instant;

/// A permit is held by each worker; dropping it releases capacity even on panic.
pub struct ConnectionPermit(Arc<AtomicUsize>);
impl ConnectionPermit {
    pub fn acquire(active: &Arc<AtomicUsize>, limit: usize) -> Option<Self> {
        let mut current = active.load(Ordering::Acquire);
        loop {
            if current >= limit {
                return None;
            }
            match active.compare_exchange_weak(
                current,
                current + 1,
                Ordering::AcqRel,
                Ordering::Acquire,
            ) {
                Ok(_) => return Some(Self(Arc::clone(active))),
                Err(actual) => current = actual,
            }
        }
    }
}
impl Drop for ConnectionPermit {
    fn drop(&mut self) {
        self.0.fetch_sub(1, Ordering::AcqRel);
    }
}

pub struct Store {
    pub accepted: u64,
    pub rejected: u64,
    pub evicted: u64,
    pub last: BTreeMap<u16, Frame>,
    history: VecDeque<Frame>,
    capacity: usize,
    received: BTreeMap<u16, Instant>,
}
impl Store {
    pub fn new(capacity: usize) -> Self {
        assert!(
            (1..=65536).contains(&capacity),
            "collector capacity must be 1..65536"
        );
        Self {
            accepted: 0,
            rejected: 0,
            evicted: 0,
            last: BTreeMap::new(),
            history: VecDeque::with_capacity(capacity),
            capacity,
            received: BTreeMap::new(),
        }
    }
    pub fn accept(&mut self, frame: Frame) -> Result<(), &'static str> {
        if self
            .last
            .get(&frame.device_id)
            .map(|prev| frame.seq <= prev.seq)
            .unwrap_or(false)
        {
            self.rejected += 1;
            return Err("out of order / replayed sequence");
        }
        if !self.last.contains_key(&frame.device_id) && self.last.len() >= MAX_DEVICES {
            self.rejected += 1;
            return Err("device limit reached");
        }
        if self.history.len() == self.capacity {
            self.history.pop_front();
            self.evicted += 1;
        }
        self.history.push_back(frame.clone());
        self.received.insert(frame.device_id, Instant::now());
        self.last.insert(frame.device_id, frame);
        self.accepted += 1;
        Ok(())
    }
    pub fn reject(&mut self) {
        self.rejected += 1;
    }
    pub fn history_len(&self) -> usize {
        self.history.len()
    }
    pub fn capacity(&self) -> usize {
        self.capacity
    }
    pub fn history(&self) -> impl Iterator<Item = &Frame> {
        self.history.iter()
    }
    pub fn age_ms(&self, id: u16) -> Option<u64> {
        self.received
            .get(&id)
            .map(|t| t.elapsed().as_millis().min(u64::MAX as u128) as u64)
    }
}

/// Validates a Modbus TCP read-holding-registers request. Only FC03, start 0,
/// exactly 6 registers are supported by the simulator's explicitly defined map.
pub fn parse_modbus_request(header: &[u8; 7], body: &[u8]) -> Result<(u16, u8), &'static str> {
    let tid = u16::from_be_bytes([header[0], header[1]]);
    if header[2] != 0 || header[3] != 0 || header[4] != 0 || header[5] != 6 || header[6] != 1 {
        return Err("invalid MBAP header / unit");
    }
    if body.len() != 5
        || body[0] != 3
        || body[1] != 0
        || body[2] != 0
        || body[3] != 0
        || body[4] != 6
    {
        return Err("unsupported Modbus request");
    }
    Ok((tid, header[6]))
}

#[cfg(test)]
mod tests {
    use super::*;
    fn bytes(seq: u64) -> [u8; FRAME_SIZE] {
        let mut raw = [0u8; FRAME_SIZE];
        raw[0..4].copy_from_slice(b"BMS1");
        raw[4..6].copy_from_slice(&1u16.to_be_bytes());
        raw[6..14].copy_from_slice(&seq.to_be_bytes());
        raw[14..22].copy_from_slice(&42u64.to_be_bytes());
        raw[22..24].copy_from_slice(&235i16.to_be_bytes());
        raw[24..26].copy_from_slice(&1020u16.to_be_bytes());
        raw[26..28].copy_from_slice(&450u16.to_be_bytes());
        raw[28] = 7;
        raw[29..31].copy_from_slice(&500u16.to_be_bytes());
        raw
    }
    #[test]
    fn parses_frame() {
        let f = Frame::parse(&bytes(1)).unwrap();
        assert_eq!(f.device_id, 1);
        assert_eq!(f.temp_deci_c, 235);
        assert_eq!(f.co2_ppm, 1020);
        assert!(f.alarm && f.hvac_on && f.occupied);
    }
    #[test]
    fn rejects_bad_frames() {
        assert!(Frame::parse(b"hello").is_err());
        let mut raw = bytes(1);
        raw[0] = 0;
        assert!(Frame::parse(&raw).is_err());
        let mut raw = bytes(1);
        raw[28] = 0x80;
        assert!(Frame::parse(&raw).is_err());
        let mut raw = bytes(1);
        raw[26..28].copy_from_slice(&1001u16.to_be_bytes());
        assert!(Frame::parse(&raw).is_err());
    }
    #[test]
    fn ring_bounded_replay_rejected() {
        let mut store = Store::new(2);
        store.accept(Frame::parse(&bytes(1)).unwrap()).unwrap();
        store.accept(Frame::parse(&bytes(2)).unwrap()).unwrap();
        store.accept(Frame::parse(&bytes(3)).unwrap()).unwrap();
        assert_eq!(
            (store.accepted, store.evicted, store.history_len()),
            (3, 1, 2)
        );
        assert!(store.accept(Frame::parse(&bytes(2)).unwrap()).is_err());
        assert_eq!(store.rejected, 1);
    }
    #[test]
    fn modbus_contract() {
        let hdr = [0, 7, 0, 0, 0, 6, 1];
        let pdu = [3, 0, 0, 0, 6];
        assert_eq!(parse_modbus_request(&hdr, &pdu), Ok((7, 1)));
        let mut wrong = pdu;
        wrong[0] = 6;
        assert!(parse_modbus_request(&hdr, &wrong).is_err());
    }
}

#[cfg(test)]
mod operational_tests {
    use super::*;
    #[test]
    fn connection_capacity_released_on_drop() {
        let count = Arc::new(AtomicUsize::new(0));
        let a = ConnectionPermit::acquire(&count, 2).unwrap();
        let b = ConnectionPermit::acquire(&count, 2).unwrap();
        assert!(ConnectionPermit::acquire(&count, 2).is_none());
        drop(a);
        assert!(ConnectionPermit::acquire(&count, 2).is_some());
        drop(b);
        assert_eq!(count.load(Ordering::Acquire), 0);
    }
}
