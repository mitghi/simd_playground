use core::arch::aarch64;
use std::arch::aarch64::{uint8x16_t, uint8x8_t};
use std::env;
use std::fs::File;
use std::io::Read;
use std::time::Instant;

#[inline(always)]
unsafe fn movemask_epi8(input: uint8x16_t) -> u64 {
    const XR: [i8; 8] = [-7, -6, -5, -4, -3, -2, -1, 0];
    let mask_and: aarch64::uint8x8_t = aarch64::vdup_n_u8(0x80);
    let mask_shift = aarch64::vld1_s8(XR.as_ptr());

    let mut lo: uint8x8_t = aarch64::vget_low_u8(input);
    let mut hi: uint8x8_t = aarch64::vget_high_u8(input);

    lo = aarch64::vand_u8(lo, mask_and);
    lo = aarch64::vshl_u8(lo, mask_shift);

    hi = aarch64::vand_u8(hi, mask_and);
    hi = aarch64::vshl_u8(hi, mask_shift);

    lo = aarch64::vpadd_u8(lo, lo);
    lo = aarch64::vpadd_u8(lo, lo);
    lo = aarch64::vpadd_u8(lo, lo);

    hi = aarch64::vpadd_u8(hi, hi);
    hi = aarch64::vpadd_u8(hi, hi);
    hi = aarch64::vpadd_u8(hi, hi);

    let a: [u8; 8] = *(&hi as *const _ as *const [u8; 8]);
    let b: [u8; 8] = *(&lo as *const _ as *const [u8; 8]);

    return (u64::from(a[0]) << 8) | (u64::from(b[0]) & 0xFF);
}

unsafe fn simd_count(input: &[u8], ch: u8) -> usize {
    let mut total: usize = 0;
    let ln = input.len();
    assert!(ln % 16 == 0);
    let tocmp: aarch64::uint8x16_t = aarch64::vdupq_n_u8(ch);
    for _input in input.chunks(16) {
        let chunk: uint8x16_t = *(_input.as_ptr() as *mut uint8x16_t);
        let results: uint8x16_t = aarch64::vceqq_u8(chunk, tocmp);
        total += movemask_epi8(results).count_ones() as usize;
    }
    return total;
}

fn naive_count(input: &[u8], ch: u8) -> usize {
    let mut total: usize = 0;
    for i in input.iter() {
        if *i == ch {
            total += 1;
        }
    }
    total
}

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() < 2 {
        panic!("first argument filepath is missing");
    }
    let filename = &args[1];
    match File::open(filename) {
        Ok(mut file) => {
            let mut content = String::new();
            let mut start = Instant::now();
            file.read_to_string(&mut content).unwrap();
            println!("[+] File loaded ( took: {:?} )", start.elapsed());

	    {
		start = Instant::now();
		let naive_result = naive_count(content.as_bytes(), String::from(",").as_bytes()[0]);
		println!("[x] Normal took: {:?}, commas: {}", start.elapsed(), naive_result);
	    }

            unsafe {
                let start = Instant::now();
                let simd_result = simd_count(content.as_bytes(), String::from(",").as_bytes()[0]);
                println!(
                    "[x] Neon_SIMD took: {:?}, commas: {}",
                    start.elapsed(),
                    simd_result
                );
            }
        }
        Err(error) => {
            panic!("{}", error);
        }
    }
}
