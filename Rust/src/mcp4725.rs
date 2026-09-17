use esp_hal::i2c::master::{Error, I2c};

pub const MCP4725_ADDR: u8 = 0x60;
pub const VREF_MV: u32 = 3300;

const SETTLE_MS: u32 = 5;

pub fn code_to_target_mv(code: u16) -> u32 {
    (code as u32 * VREF_MV) / 4096
}

pub fn dac_code_for_step(step: usize, total_steps: usize) -> u16 {
    if total_steps <= 1 {
        return 0;
    }
    if step >= total_steps - 1 {
        return 4095;
    }
    ((step as u32 * 4095) / (total_steps as u32 - 1)) as u16
}

fn write_fast(i2c: &mut I2c<'_, esp_hal::Blocking>, code: u16) -> Result<(), Error> {
    let code = code & 0x0FFF;
    let buf = [(code >> 8) as u8 & 0x0F, code as u8];
    i2c.write(MCP4725_ADDR, &buf)
}

fn write_register(i2c: &mut I2c<'_, esp_hal::Blocking>, code: u16) -> Result<(), Error> {
    let code = code & 0x0FFF;
    let buf = [0x40, (code >> 4) as u8, ((code & 0x0F) << 4) as u8];
    i2c.write(MCP4725_ADDR, &buf)
}

pub fn read_code(i2c: &mut I2c<'_, esp_hal::Blocking>) -> Result<u16, Error> {
    let mut data = [0u8; 3];
    i2c.read(MCP4725_ADDR, &mut data)?;
    Ok(((data[1] as u16) << 4) | (data[2] as u16 >> 4))
}

pub fn set_code(i2c: &mut I2c<'_, esp_hal::Blocking>, code: u16, delay: &mut esp_hal::delay::Delay) -> Result<(), Error> {
    write_fast(i2c, code)?;
    delay.delay_millis(SETTLE_MS);

    if read_code(i2c)? == code {
        return Ok(());
    }

    esp_println::println!("mcp4725: fast write rb mismatch, trying register write");
    write_register(i2c, code)?;
    delay.delay_millis(SETTLE_MS);
    Ok(())
}
