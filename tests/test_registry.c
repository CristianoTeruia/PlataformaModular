/* Teste no PC (tests/test_registry.c): (1) equivalencia V10 x tabela para os 3 sensores antigos; (2) drivers do BH1750 e
 * do ENS160 contra dispositivos I2C SIMULADOS; (3) registro/CSV com 5 sensores. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "main.h"
#include "app_state.h"
#include "sensor_registry.h"
#include "bh1750.h"
#include "ens160.h"

static long fails = 0, checks = 0;
#define CHECK(cond, ...) do { checks++; if (!(cond)) { fails++; printf("FALHA: " __VA_ARGS__); printf("\n"); } } while (0)

/* ---------------- globais/stubs dos sensores antigos ---------------- */
float aht20_temperature, aht20_humidity, bmp280_temperature, bmp280_pressure;
bool aht20_available, bmp280_available, mpu6500_available;
uint8_t aht20_last_read_ok, bmp280_last_read_ok, mpu6500_last_read_ok;
int16_t mpu6500_accel_z;
float mpu6500_accel_x_g, mpu6500_accel_y_g, mpu6500_accel_z_g, mpu6500_gyro_x_dps, mpu6500_gyro_y_dps, mpu6500_gyro_z_dps;
int32_t display_aht_temp, display_aht_hum, display_pressure, display_accel_z;
HAL_StatusTypeDef AHT20_Read(float *t, float *h){ (void)t;(void)h; return HAL_OK; }
HAL_StatusTypeDef BMP280_ReadCalibration(void){ return HAL_OK; }
HAL_StatusTypeDef BMP280_Read(float *t, float *p){ (void)t;(void)p; return HAL_OK; }
/* MPU6500 simulado: registra a ordem das chamadas (I=Init C=Calibrate R=Read S=Sleep W=Wake). */
static char mpu_log[64]; static int mpu_n = 0;
static int mpu_fail_init = 0, mpu_fail_wake = 0, mpu_fail_read = 0;
static void mlog(char c){ if (mpu_n < 62) { mpu_log[mpu_n++] = c; mpu_log[mpu_n] = 0; } }
static void mpu_clear_log(void){ mpu_n = 0; mpu_log[0] = 0; }
HAL_StatusTypeDef MPU6500_Init(void){ mlog('I'); return mpu_fail_init ? HAL_ERROR : HAL_OK; }
HAL_StatusTypeDef MPU6500_Calibrate(void){ mlog('C'); return HAL_OK; }
HAL_StatusTypeDef MPU6500_Read(void){ mlog('R'); return mpu_fail_read ? HAL_ERROR : HAL_OK; }
HAL_StatusTypeDef MPU6500_Sleep(void){ mlog('S'); return HAL_OK; }
HAL_StatusTypeDef MPU6500_Wake(void){ mlog('W'); return mpu_fail_wake ? HAL_ERROR : HAL_OK; }

/* ---------------- barramento I2C simulado ---------------- */
I2C_HandleTypeDef hi2c1;
static uint32_t fake_ms = 0;
void App_Delay(uint32_t ms){ fake_ms += ms; }

/* BH1750 simulado */
static uint8_t bh_present = 0;          /* endereco 7 bits (0 = ausente) */
static uint16_t bh_raw_value = 0;
static uint8_t bh_cmds[16]; static int bh_ncmds = 0; static uint8_t bh_last_cmd = 0;

/* ENS160 simulado, ESTRITO: STANDARD so vale se o sensor esta em IDLE e o flag NEWGPR ja foi limpo
 * (CLRGPR); depois de iniciar, gera uma medicao por segundo (NEWDAT). Reproduz o caso real em que o
 * sensor responde (PART_ID ok, status 0x01) mas nao mede quando recebe STANDARD direto. */
static struct {
    uint8_t present, opmode, running, newgpr, newdat, error, validity, aqi, never_runs, no_statas, newgpr_after_run, stall, dead, newdat_once, written, gpr_ver;
    uint32_t latency;
    uint16_t tvoc, eco2; uint8_t temp_in[2], rh_in[2]; uint32_t next_data_ms;
    uint8_t opmode_log[32]; int nop; uint8_t cmd_log[32]; int ncmd;
} E;
static void ens_power_on(uint8_t addr)
{ memset(&E, 0, sizeof(E)); E.present = addr; E.opmode = 0x00; E.newgpr = 1; E.latency = 1000; ens160_addr = 0; }
static uint8_t ens_status(void)
{
    if (E.newdat_once) { E.newdat = 1; E.newdat_once = 0; }
    if (E.running && !E.stall && fake_ms >= E.next_data_ms) { E.newdat = 1; E.next_data_ms = fake_ms + 1000; }
    return (uint8_t)(((E.running && !E.no_statas) ? 0x80 : 0) | (E.error ? 0x40 : 0) | ((E.validity & 3) << 2) | (E.newdat ? 2 : 0) | (E.newgpr ? 1 : 0));
}
static int ens_log_has(uint8_t v){ for (int i = 0; i < E.nop; i++) if (E.opmode_log[i] == v) return 1; return 0; }

HAL_StatusTypeDef HAL_I2C_Master_Transmit(I2C_HandleTypeDef *h, uint16_t addr, uint8_t *d, uint16_t n, uint32_t t)
{ (void)h;(void)t; if (bh_present == 0 || (addr >> 1) != bh_present || n != 1) return HAL_ERROR;
  bh_last_cmd = d[0]; if (bh_ncmds < 16) bh_cmds[bh_ncmds++] = d[0]; return HAL_OK; }
HAL_StatusTypeDef HAL_I2C_Master_Receive(I2C_HandleTypeDef *h, uint16_t addr, uint8_t *d, uint16_t n, uint32_t t)
{ (void)h;(void)t; if (bh_present == 0 || (addr >> 1) != bh_present || n != 2 || bh_last_cmd != 0x20) return HAL_ERROR;
  d[0] = (uint8_t)(bh_raw_value >> 8); d[1] = (uint8_t)(bh_raw_value & 0xFF); return HAL_OK; }
HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef *h, uint16_t addr, uint16_t reg, uint16_t sz, uint8_t *d, uint16_t n, uint32_t t)
{ (void)h;(void)sz;(void)t; if (E.present == 0 || (addr >> 1) != E.present) return HAL_ERROR;
  for (uint16_t i = 0; i < n; i++) {
      uint16_t r = reg + i; uint8_t v = 0;
      if (r == 0x00) v = 0x60; else if (r == 0x01) v = 0x01; else if (r == 0x10) v = E.opmode;
      else if (r == 0x20) v = ens_status();
      else if (r == 0x21) v = E.aqi; else if (r == 0x22) v = E.tvoc & 0xFF; else if (r == 0x23) v = E.tvoc >> 8;
      else if (r == 0x24) v = E.eco2 & 0xFF; else if (r == 0x25) v = E.eco2 >> 8;
      else if (r == 0x4C) v = E.gpr_ver ? 5 : 0; else if (r == 0x4D) v = E.gpr_ver ? 6 : 0; else if (r == 0x4E) v = E.gpr_ver ? 7 : 0;
      else if (r == 0x30) v = (E.running && E.written) ? E.temp_in[0] : 0x89; else if (r == 0x31) v = (E.running && E.written) ? E.temp_in[1] : 0x4A;
      else if (r == 0x32) v = (E.running && E.written) ? E.rh_in[0] : 0x00; else if (r == 0x33) v = (E.running && E.written) ? E.rh_in[1] : 0x64;
      d[i] = v; }
  if (reg <= 0x21 && reg + n > 0x21) E.newdat = 0;   /* ler os dados consome o NEWDAT */
  return HAL_OK; }
HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef *h, uint16_t addr, uint16_t reg, uint16_t sz, uint8_t *d, uint16_t n, uint32_t t)
{ (void)h;(void)sz;(void)t; if (E.present == 0 || (addr >> 1) != E.present) return HAL_ERROR;
  for (uint16_t i = 0; i < n; i++) {
      uint16_t r = reg + i; uint8_t v = d[i];
      if (r == 0x10) {
          if (E.nop < 32) E.opmode_log[E.nop++] = v;
          if (v == 0xF0) { E.opmode = 0x00; E.running = 0; E.newgpr = 1; E.newdat = 0; }
          else if (v == 0x01) { E.opmode = 0x01; E.running = 0; }
          else if (v == 0x00) { E.opmode = 0x00; E.running = 0; }
          else if (v == 0x02 && E.opmode == 0x01 && E.newgpr == 0 && !E.never_runs) {
              E.opmode = 0x02; E.running = 1; E.next_data_ms = fake_ms + E.latency; if (E.newgpr_after_run) E.newgpr = 1;
              if (E.dead) { E.stall = 1; E.newdat_once = 1; } }
      } else if (r == 0x12) {
          if (E.ncmd < 32) E.cmd_log[E.ncmd++] = v;
          if (v == 0xCC && E.opmode == 0x01) E.newgpr = 0;
          if (v == 0x0E && E.opmode == 0x01) E.gpr_ver = 1;
      } else if (r == 0x13) { E.temp_in[0] = v; E.written = 1; } else if (r == 0x14) E.temp_in[1] = v;
      else if (r == 0x15) E.rh_in[0] = v; else if (r == 0x16) E.rh_in[1] = v; }
  return HAL_OK; }

/* ---------------- REFERENCIA V10 (3 sensores antigos) ---------------- */
static uint16_t ref_LE16(const uint8_t *p){ return (uint16_t)p[0] | ((uint16_t)p[1] << 8); }
static uint32_t ref_LE32(const uint8_t *p){ return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24); }
static int ref_AppendScaled(char *b, size_t cap, int cur, int32_t v, uint32_t scale, uint8_t dec)
{ uint32_t mag; const char *sign = ""; int64_t w = v; if (w < 0) { sign = "-"; w = -w; } mag = (uint32_t)w;
  return snprintf(b + cur, cap - (size_t)cur, "%s%lu.%0*lu", sign, (unsigned long)(mag / scale), (int)dec, (unsigned long)(mag % scale)); }
static uint8_t ref_RecordSizeFromMask(uint8_t mask, uint8_t *size)
{
    uint16_t c = 1U + 7U + 2U;
    if ((size == NULL) || (mask == 0U) || ((mask & (uint8_t)~0x07U) != 0U)) return 0U;
    if (mask & SENSOR_AHT20)   { c += 4U; }
    if (mask & SENSOR_BMP280)  { c += 4U; }
    if (mask & SENSOR_MPU6500) { c += 12U; }
    if (c > EEPROM_MAX_RECORD_SIZE) return 0U;
    *size = (uint8_t)c;
    return 1U;
}
static uint8_t ref_serialize(uint8_t m, uint8_t *buf, uint8_t idx)
{
  if (m & SENSOR_AHT20) { int16_t t = (int16_t)(aht20_temperature * 100.0f); uint16_t h = (uint16_t)(aht20_humidity * 100.0f);
      memcpy(&buf[idx], &t, 2); idx += 2; memcpy(&buf[idx], &h, 2); idx += 2; }
  if (m & SENSOR_BMP280) { uint32_t p = (uint32_t)bmp280_pressure; memcpy(&buf[idx], &p, 4); idx += 4; }
  if (m & SENSOR_MPU6500) { int16_t v[6] = { (int16_t)(mpu6500_accel_x_g * 1000.0f), (int16_t)(mpu6500_accel_y_g * 1000.0f),
      (int16_t)(mpu6500_accel_z_g * 1000.0f), (int16_t)(mpu6500_gyro_x_dps * 100.0f), (int16_t)(mpu6500_gyro_y_dps * 100.0f),
      (int16_t)(mpu6500_gyro_z_dps * 100.0f) }; for (int i = 0; i < 6; i++) { memcpy(&buf[idx], &v[i], 2); idx += 2; } }
  return idx;
}
static int ref_csv(uint8_t m, const uint8_t *r, char *line, size_t cap)
{
  char t_[20]="", h_[20]="", p_[20]="", ax[20]="", ay[20]="", az[20]="", gx[20]="", gy[20]="", gz[20]="";
  uint16_t i = 1; int has_aht = !!(m & SENSOR_AHT20), has_bmp = !!(m & SENSOR_BMP280), has_mpu = !!(m & SENSOR_MPU6500);
  int16_t at=0, x=0, y=0, z=0, a=0, b=0, c=0; uint16_t ah = 0; uint32_t bp = 0;
  if (has_aht) { at = (int16_t)ref_LE16(&r[i]); i += 2; ah = ref_LE16(&r[i]); i += 2; }
  if (has_bmp) { bp = ref_LE32(&r[i]); i += 4; }
  if (has_mpu) { x=(int16_t)ref_LE16(&r[i]); i+=2; y=(int16_t)ref_LE16(&r[i]); i+=2; z=(int16_t)ref_LE16(&r[i]); i+=2;
                 a=(int16_t)ref_LE16(&r[i]); i+=2; b=(int16_t)ref_LE16(&r[i]); i+=2; c=(int16_t)ref_LE16(&r[i]); i+=2; }
  if (has_aht) { ref_AppendScaled(t_, 20, 0, at, 100, 2); ref_AppendScaled(h_, 20, 0, ah, 100, 2); }
  if (has_bmp) snprintf(p_, 20, "%lu", (unsigned long)bp);
  if (has_mpu) { ref_AppendScaled(ax,20,0,x,1000,3); ref_AppendScaled(ay,20,0,y,1000,3); ref_AppendScaled(az,20,0,z,1000,3);
                 ref_AppendScaled(gx,20,0,a,100,2); ref_AppendScaled(gy,20,0,b,100,2); ref_AppendScaled(gz,20,0,c,100,2); }
  return snprintf(line, cap, "%u,%s,%u,%s,%s,%s,%s,%s,%s,%s,%s,%s", (unsigned)m, "2026-10-30 12:00:00", (unsigned)has_aht,
                  t_, h_, p_, ax, ay, az, gx, gy, gz);
}
static float rnd(float lo, float hi){ return lo + (hi - lo) * ((float)rand() / (float)RAND_MAX); }

int main(void)
{
    char buf[400];
    srand(777);

    /* ===== 1. EQUIVALENCIA com a V10 para os 3 sensores antigos ===== */
    for (int m = 0; m < 256; m++) {
        uint8_t a = 0, b = 0; uint8_t ra = ref_RecordSizeFromMask((uint8_t)m, &a), rb = Sensors_RecordSizeFromMask((uint8_t)m, &b);
        if (m <= 7) { CHECK(ra == rb && (!ra || a == b), "tamanho mascara %d", m); }
        else { int valid = (m & ~0x1F) == 0; CHECK(rb == (uint8_t)valid, "mascara %d validade esperada %d", m, valid); }
    }
    for (int iter = 0; iter < 40000; iter++) {
        int ext = (iter % 10 == 0);
        aht20_temperature = ext ? rnd(-50.f, 150.f) : rnd(-10.f, 50.f); aht20_humidity = rnd(0.f, 100.f);
        bmp280_pressure = ext ? rnd(0.f, 130000.f) : rnd(90000.f, 105000.f);
        mpu6500_accel_x_g = rnd(-16.f, 16.f) * (ext ? 1.f : .1f); mpu6500_accel_y_g = rnd(-16.f, 16.f) * (ext ? 1.f : .1f);
        mpu6500_accel_z_g = rnd(-16.f, 16.f) * (ext ? 1.f : .1f);
        mpu6500_gyro_x_dps = rnd(-320.f, 320.f); mpu6500_gyro_y_dps = rnd(-320.f, 320.f); mpu6500_gyro_z_dps = rnd(-320.f, 320.f);
        for (uint8_t m = 1; m <= 7; m++) {
            uint8_t ra[EEPROM_MAX_RECORD_SIZE] = {0}, rb[EEPROM_MAX_RECORD_SIZE] = {0};
            ra[0] = rb[0] = m;
            uint8_t ia = ref_serialize(m, ra, 1), ib = Sensors_Serialize(m, rb, 1);
            CHECK(ia == ib && memcmp(ra, rb, EEPROM_MAX_RECORD_SIZE) == 0, "bytes m=%u", m);
            char la[300], lb[300]; ref_csv(m, ra, la, sizeof(la));
            /* a nova linha tem as colunas dos sensores novos vazias no fim: ",,,,," (bh + ens) */
            int nb = snprintf(lb, sizeof(lb), "%u,%s,", (unsigned)m, "2026-10-30 12:00:00");
            int r = Sensors_CsvFormatRow(m, &rb[1], lb + nb, sizeof(lb) - (size_t)nb);
            CHECK(r >= 0, "csv r");
            char expected[320]; snprintf(expected, sizeof(expected), "%s,,,,,", la);
            CHECK(strcmp(expected, lb) == 0, "csv m=%u\n ref=%s\n new=%s", m, expected, lb);
        }
    }

    /* ===== 2. BH1750 simulado ===== */
    uint16_t raw = 0;
    bh_present = 0; bh1750_addr = 0;
    CHECK(BH1750_Read(&raw) != HAL_OK && bh1750_addr == 0, "bh ausente deve falhar");
    bh_present = 0x5C; bh_raw_value = 1200; bh_ncmds = 0;
    CHECK(BH1750_Read(&raw) == HAL_OK && raw == 1200 && bh1750_addr == 0x5C, "bh em 0x5C: raw=%u addr=%x", raw, bh1750_addr);
    CHECK(bh_ncmds >= 2 && bh_cmds[bh_ncmds - 2] == 0x01 && bh_cmds[bh_ncmds - 1] == 0x20, "sequencia de comandos 0x01 depois 0x20");
    bh_present = 0x23; bh1750_addr = 0; bh_raw_value = 0xFFFF;
    CHECK(BH1750_Read(&raw) == HAL_OK && raw == 0xFFFF && bh1750_addr == 0x23, "bh em 0x23");
    bh_present = 0;   /* removido */
    CHECK(BH1750_Read(&raw) != HAL_OK && bh1750_addr == 0, "bh removido: endereco deve zerar");
    bh_present = 0x23; bh_raw_value = 77;
    CHECK(BH1750_Read(&raw) == HAL_OK && raw == 77, "bh recolocado deve recuperar");

    /* ===== 3. ENS160 simulado (estrito, com os comportamentos observados no modulo real) ===== */
    ENS160_Sample es; uint32_t t0;
    ens_power_on(0);
    CHECK(ENS160_Read(false, 0, 0, &es) != HAL_OK && ens160_addr == 0, "ens ausente deve falhar");

    /* 3a. CASO REAL: repouso com NEWGPR; o bit 0x80 nunca liga e NEWGPR volta a ligar (status 0x03) */
    ens_power_on(0x53); E.no_statas = 1; E.newgpr_after_run = 1; E.aqi = 3; E.tvoc = 123; E.eco2 = 612; E.validity = 1;
    t0 = fake_ms;
    CHECK(ENS160_Read(true, 25.0f, 50.0f, &es) == HAL_OK, "ens deve sair do repouso e medir (caso real)");
    CHECK(ens160_addr == 0x53 && ens160_part_id == 0x0160, "addr/part_id");
    CHECK(es.aqi == 3 && es.tvoc_ppb == 123 && es.eco2_ppm == 612 && es.validity == 1, "valores %u %u %u %u", es.aqi, es.tvoc_ppb, es.eco2_ppm, es.validity);
    CHECK(E.nop == 2 && E.opmode_log[0] == 0x01 && E.opmode_log[1] == 0x02, "IDLE e depois STANDARD, sem RESET (n=%d)", E.nop);
    CHECK(E.ncmd == 3 && E.cmd_log[0] == 0x00 && E.cmd_log[1] == 0xCC && E.cmd_log[2] == 0x0E, "NOP, CLRGPR e GET_APPVER antes de STANDARD (n=%d)", E.ncmd);
    CHECK(E.temp_in[0] == 0x89 && E.temp_in[1] == 0x4A, "TEMP_IN 0x4A89: %02x%02x", E.temp_in[1], E.temp_in[0]);
    CHECK(E.rh_in[0] == 0x00 && E.rh_in[1] == 0x64, "RH_IN 0x6400: %02x%02x", E.rh_in[1], E.rh_in[0]);
    CHECK(ens160_init_count >= 1 && (ens160_last_status & 0x80) == 0 && (ens160_last_status & 0x02) != 0, "dado aceito sem o bit 0x80 e com NEWDAT: status=%02x", ens160_last_status);
    CHECK(ens160_last_aqi == 3 && ens160_last_tvoc == 123 && ens160_last_eco2 == 612 && ens160_last_validity == 1, "ultima amostra exposta no DIAG");
    CHECK(fake_ms - t0 < 3500, "espera limitada (%u ms)", (unsigned)(fake_ms - t0));

    /* 3b. REGIME NORMAL: varias leituras seguidas NAO reinicializam (corrige o ensinit=4 da V12b) */
    { uint32_t init_before = ens160_init_count, rst_before = ens160_reset_count; int nop_before = E.nop;
      for (int k = 0; k < 8; k++) { fake_ms += 10000; E.aqi = (uint8_t)(1 + k % 5);
          CHECK(ENS160_Read(false, 0, 0, &es) == HAL_OK && es.aqi == E.aqi, "regime normal k=%d", k); }
      CHECK(ens160_init_count == init_before && ens160_reset_count == rst_before && E.nop == nop_before, "regime normal nao reinicializa"); }

    /* 3c. a placa reinicia mas o sensor continua medindo: nao mexe nele */
    { uint32_t init_before = ens160_init_count; int nop_before = E.nop; ens160_addr = 0; fake_ms += 10000;
      CHECK(ENS160_Read(false, 0, 0, &es) == HAL_OK, "sensor ja medindo");
      CHECK(ens160_init_count == init_before && E.nop == nop_before, "nao deve mexer num sensor que ja mede"); }

    /* 3d. primeira medicao lenta (2,5 s): a janela de 3,2 s apos iniciar deve bastar */
    ens_power_on(0x53); E.no_statas = 1; E.latency = 2500; E.aqi = 2; E.tvoc = 40; E.eco2 = 450; t0 = fake_ms;
    CHECK(ENS160_Read(false, 0, 0, &es) == HAL_OK && es.aqi == 2, "1a medicao lenta (2,5 s)");
    CHECK(ens160_init_count >= 1, "iniciou");

    /* 3e. sensor medindo que deixa de entregar dado: NAO reinicia a cada ciclo; so apos 3 ciclos, com RESET */
    ens_power_on(0x53); E.no_statas = 1; E.aqi = 1; E.tvoc = 5; E.eco2 = 400;
    CHECK(ENS160_Read(false, 0, 0, &es) == HAL_OK, "e3 inicia");
    { uint32_t init_before = ens160_init_count; E.stall = 1;
      CHECK(ENS160_Read(false, 0, 0, &es) == HAL_BUSY, "e3 ciclo 1 sem dado");
      CHECK(ENS160_Read(false, 0, 0, &es) == HAL_BUSY, "e3 ciclo 2 sem dado");
      CHECK(ens160_init_count == init_before && !ens_log_has(0xF0), "e3 nao reinicia nos 2 primeiros ciclos");
      CHECK(ENS160_Read(false, 0, 0, &es) == HAL_BUSY, "e3 ciclo 3 sem dado");
      E.stall = 0;
      CHECK(ENS160_Read(false, 0, 0, &es) == HAL_OK && ens_log_has(0xF0), "e3 apos 3 ciclos: RESET e volta a medir");
      CHECK(ens160_init_count == init_before + 1, "e3 uma unica reinicializacao"); }

    /* 3f. sensor que nunca entra em operacao: BUSY sem travar; a tentativa seguinte usa RESET */
    ens_power_on(0x53); E.never_runs = 1; t0 = fake_ms;
    CHECK(ENS160_Read(false, 0, 0, &es) == HAL_BUSY && ens160_addr == 0x53, "nunca mede: BUSY sem perder o endereco");
    CHECK(fake_ms - t0 < 4000, "espera limitada no caso ruim (%u ms)", (unsigned)(fake_ms - t0));
    CHECK(!ens_log_has(0xF0), "1a tentativa nao usa RESET");
    CHECK(ENS160_Read(false, 0, 0, &es) == HAL_BUSY && ens_log_has(0xF0), "2a tentativa usa RESET");
    E.never_runs = 0; E.aqi = 2; E.tvoc = 50; E.eco2 = 450;
    CHECK(ENS160_Read(false, 0, 0, &es) == HAL_OK && es.aqi == 2, "recupera quando o sensor passa a funcionar");

    /* 3g. erro interno (STATER): erro e depois RESET */
    ens_power_on(0x52); E.aqi = 1; E.tvoc = 10; E.eco2 = 400;
    CHECK(ENS160_Read(false, 0, 0, &es) == HAL_OK && ens160_addr == 0x52, "ens em 0x52");
    E.error = 1; { uint32_t errs = ens160_error_count;
      CHECK(ENS160_Read(false, 0, 0, &es) == HAL_ERROR && ens160_error_count == errs + 1, "STATER deve dar erro"); }
    E.error = 0;
    CHECK(ENS160_Read(false, 0, 0, &es) == HAL_OK && ens_log_has(0xF0), "apos erro: RESET e volta a medir");

    /* 3h. AQI usa 3 bits; validade; limites da compensacao */
    ens_power_on(0x53); E.aqi = 0xFD; E.tvoc = 0xFFFF; E.eco2 = 0x1234; E.validity = 3;
    CHECK(ENS160_Read(true, -100.f, 150.f, &es) == HAL_OK, "extremos");
    CHECK(es.aqi == 5 && es.tvoc_ppb == 0xFFFF && es.eco2_ppm == 0x1234 && es.validity == 3, "extremos: %u %u %u %u", es.aqi, es.tvoc_ppb, es.eco2_ppm, es.validity);
    CHECK(E.temp_in[0] == (uint8_t)((uint16_t)((-40.f + 273.15f) * 64.f) & 0xFF) && E.rh_in[1] == 0xC8, "limites da compensacao");

    /* 3i. removido / endereco sem ENS160 */
    { uint32_t comm = ens160_comm_fail_count; E.present = 0;
      CHECK(ENS160_Read(false, 0, 0, &es) != HAL_OK && ens160_addr == 0, "ens removido: endereco zera");
      (void)comm; }
    ens_power_on(0x53); E.present = 0x40;
    CHECK(ENS160_Read(false, 0, 0, &es) != HAL_OK && ens160_addr == 0, "endereco sem ENS160");

    /* 3j. versao do firmware do sensor e eco de temperatura/umidade (diagnostico) */
    ens_power_on(0x53); E.no_statas = 1; E.aqi = 2; E.tvoc = 30; E.eco2 = 480;
    CHECK(ENS160_Read(true, 22.4f, 69.0f, &es) == HAL_OK, "j: leitura");
    CHECK(ens160_fw_major == 5 && ens160_fw_minor == 6 && ens160_fw_build == 7, "j: firmware %u.%u.%u", ens160_fw_major, ens160_fw_minor, ens160_fw_build);
    fake_ms += 10000;
    CHECK(ENS160_Read(true, 22.4f, 69.0f, &es) == HAL_OK, "j: leitura 2");
    { uint16_t t_raw = (uint16_t)((22.4f + 273.15f) * 64.0f), rh_raw = (uint16_t)(69.0f * 512.0f);
      CHECK(ens160_echo_t == t_raw && ens160_echo_rh == rh_raw, "j: eco T/UR %u/%u (esperado %u/%u)", ens160_echo_t, ens160_echo_rh, t_raw, rh_raw); }

    /* 3k. CASO DA BANCADA (V12c): so UM "dado novo" com zeros logo apos iniciar e depois silencio */
    ens_power_on(0x53); E.no_statas = 1; E.dead = 1;   /* aqi/tvoc/eco2 = 0 */
    { uint32_t zero0 = ens160_zero_count, init0 = ens160_init_count; int saw_ok = 0, saw_reset_after = 0;
      t0 = fake_ms;
      for (int cycle = 0; cycle < 12; cycle++) {
          fake_ms += 10000;
          Sensors_ReadOne(4);
          if (Sensors_AvailableMask() & SENSOR_ENS160) saw_ok = 1;
          if (cycle >= 3 && ens_log_has(0xF0)) saw_reset_after = 1;
      }
      CHECK(!saw_ok, "k: zeros nunca devem virar amostra valida");
      CHECK((Sensors_DetectedMask() & SENSOR_ENS160) != 0, "k: sensor presente mas sem dado deve contar como DETECTADO (OLED mostra erro)");
      { const char *nm = Sensors_FirstMissingName(Sensors_DetectedMask(), Sensors_AvailableMask());
        CHECK(nm != NULL && strcmp(nm, "ENS160") == 0, "k: OLED deve apontar ERRO:ENS160"); }
      CHECK(ens160_zero_count > zero0, "k: leituras zeradas contadas (%u)", (unsigned)(ens160_zero_count - zero0));
      CHECK(saw_reset_after, "k: apos varios ciclos sem dado reinicializa com RESET");
      CHECK(ens160_init_count - init0 >= 2 && ens160_init_count - init0 <= 5, "k: ~1 reinicializacao a cada 3 ciclos, nao a cada ciclo (%u em 12 ciclos)", (unsigned)(ens160_init_count - init0)); }

    /* ===== 4. Registro com 5 sensores ===== */
    CHECK(Sensors_CsvHeader(buf, sizeof(buf)) > 0, "cabecalho");
    CHECK(strcmp(buf, "sensor_mask,timestamp,aht_present,aht_temp_c,aht_humidity_pct,bmp_pressure,accel_x_g,accel_y_g,accel_z_g,"
                      "gyro_x_dps,gyro_y_dps,gyro_z_dps,bh_lux,ens_aqi,ens_tvoc_ppb,ens_eco2_ppm,ens_validity\r\n") == 0, "texto do cabecalho: %s", buf);
    uint8_t sz = 0;
    CHECK(Sensors_RecordSizeFromMask(0x1F, &sz) && sz == 38 && sz <= EEPROM_MAX_RECORD_SIZE, "registro completo tem 38 bytes (%u)", sz);
    CHECK(Sensors_RecordSizeFromMask(0x18, &sz) && sz == 1 + 2 + 6 + 7 + 2, "so BH+ENS: %u", sz);
    CHECK(!Sensors_RecordSizeFromMask(0x20, &sz), "bit 5 e invalido");
    /* valores: AHT 21,50C/60,00%  BMP 93500  MPU 0,010/-0,020/1,000 g, gyro 1,00/-2,00/0,50  BH raw 1200  ENS 3/123/612/1 */
    aht20_temperature = 21.5f; aht20_humidity = 60.0f; bmp280_pressure = 93500.f;
    mpu6500_accel_x_g = 0.010f; mpu6500_accel_y_g = -0.020f; mpu6500_accel_z_g = 1.0f;
    mpu6500_gyro_x_dps = 1.0f; mpu6500_gyro_y_dps = -2.0f; mpu6500_gyro_z_dps = 0.5f;
    bh_present = 0x23; bh_raw_value = 1200; ens_power_on(0x53); E.no_statas = 1; E.aqi = 3; E.tvoc = 123; E.eco2 = 612; E.validity = 1;
    for (uint8_t i = 0; i < Sensors_Count(); i++) Sensors_ReadOne(i);
    CHECK(Sensors_AvailableMask() == 0x1F && Sensors_DetectedMask() == 0x1F, "mascaras apos leitura: %x %x", Sensors_AvailableMask(), Sensors_DetectedMask());
    uint8_t rec[EEPROM_MAX_RECORD_SIZE] = {0}; rec[0] = 0x1F;
    uint8_t end = Sensors_Serialize(0x1F, rec, 1);
    CHECK(end == 1 + 4 + 4 + 12 + 2 + 6, "bytes serializados: %u", end);
    char row[300]; int r = Sensors_CsvFormatRow(0x1F, &rec[1], row, sizeof(row));
    CHECK(r > 0 && strcmp(row, "1,21.50,60.00,93500,0.010,-0.020,1.000,1.00,-2.00,0.50,1000.0,3,123,612,1") == 0, "linha completa: %s", row);
    /* mascaras parciais: sensores ausentes viram campos vazios, na posicao certa */
    r = Sensors_CsvFormatRow(0x18, &rec[1 + 4 + 4 + 12], row, sizeof(row));
    CHECK(r > 0 && strcmp(row, "0,,,,,,,,,,1000.0,3,123,612,1") == 0, "so BH+ENS: %s", row);
    r = Sensors_CsvFormatRow(0x08, &rec[1 + 4 + 4 + 12], row, sizeof(row));
    CHECK(r > 0 && strcmp(row, "0,,,,,,,,,,1000.0,,,,") == 0, "so BH: %s", row);
    /* contagem de colunas igual em todas as mascaras validas */
    for (int m = 1; m <= 0x1F; m++) {
        uint8_t rr[EEPROM_MAX_RECORD_SIZE] = {0}; uint8_t e2 = Sensors_Serialize((uint8_t)m, rr, 1); (void)e2;
        r = Sensors_CsvFormatRow((uint8_t)m, &rr[1], row, sizeof(row));
        int commas = 0; for (char *q = row; *q; q++) if (*q == ',') commas++;
        CHECK(r > 0 && commas == 14, "mascara %d: %d virgulas (esperado 14)", m, commas);
    }
    /* lux: conversao e arredondamento */
    uint8_t one_bh[2];
    const struct { uint16_t raw; const char *lux; } lux_cases[] = { {0,"0.0"}, {1,"0.8"}, {5,"4.2"}, {6,"5.0"}, {11,"9.2"}, {1200,"1000.0"}, {65535,"54612.5"} };
    for (unsigned i = 0; i < sizeof(lux_cases)/sizeof(lux_cases[0]); i++) {
        one_bh[0] = lux_cases[i].raw & 0xFF; one_bh[1] = lux_cases[i].raw >> 8;
        r = Sensors_CsvFormatRow(0x08, one_bh, row, sizeof(row));
        char exp[64]; snprintf(exp, sizeof(exp), "0,,,,,,,,,,%s,,,,", lux_cases[i].lux);
        CHECK(r > 0 && strcmp(row, exp) == 0, "lux raw=%u: %s (esperado %s)", lux_cases[i].raw, row, exp);
    }
    /* falha de um sensor: sai da mascara disponivel, continua em detectado */
    bh_present = 0; Sensors_ReadOne(3);
    CHECK(Sensors_AvailableMask() == 0x17 && Sensors_DetectedMask() == 0x1F, "BH removido: %x %x", Sensors_AvailableMask(), Sensors_DetectedMask());
    { const char *nm = Sensors_FirstMissingName(Sensors_DetectedMask(), Sensors_AvailableMask());
      CHECK(nm != NULL && strcmp(nm, "BH1750") == 0, "nome do sensor ausente"); }
    CHECK(Sensors_PopCount(0x1F) == 5 && Sensors_PopCount(0) == 0 && Sensors_PopCount(0xFF) == 8, "popcount");

    /* ===== 5. MPU6500: dorme entre as amostras (economia de energia) ===== */
    {
        const uint8_t MPU_IDX = 2;
        mpu6500_available = false; mpu_fail_init = mpu_fail_wake = mpu_fail_read = 0;
        mpu_clear_log(); Sensors_ReadOne(MPU_IDX);
        CHECK(strcmp(mpu_log, "ICRS") == 0, "mpu: 1o ciclo = Init,Calibrate,Read,Sleep (obtido %s)", mpu_log);
        CHECK((Sensors_AvailableMask() & SENSOR_MPU6500) != 0, "mpu: disponivel apos o 1o ciclo");
        mpu_clear_log(); Sensors_ReadOne(MPU_IDX);
        CHECK(strcmp(mpu_log, "WRS") == 0, "mpu: ciclo normal = Wake,Read,Sleep (obtido %s)", mpu_log);
        mpu_clear_log(); Sensors_ReadOne(MPU_IDX); Sensors_ReadOne(MPU_IDX);
        CHECK(strcmp(mpu_log, "WRSWRS") == 0, "mpu: sempre dorme apos cada leitura (%s)", mpu_log);

        mpu_fail_wake = 1; mpu_clear_log(); Sensors_ReadOne(MPU_IDX);
        CHECK(strcmp(mpu_log, "WS") == 0, "mpu: falha ao acordar nao le, mas tenta dormir (%s)", mpu_log);
        CHECK((Sensors_AvailableMask() & SENSOR_MPU6500) == 0, "mpu: indisponivel apos falha ao acordar");
        mpu_fail_wake = 0; mpu_clear_log(); Sensors_ReadOne(MPU_IDX);
        CHECK(strcmp(mpu_log, "ICRS") == 0, "mpu: apos falha reinicializa (%s)", mpu_log);
        CHECK((Sensors_AvailableMask() & SENSOR_MPU6500) != 0, "mpu: volta a ficar disponivel");

        mpu_fail_read = 1; mpu_clear_log(); Sensors_ReadOne(MPU_IDX);
        CHECK(strcmp(mpu_log, "WRS") == 0, "mpu: falha de leitura ainda volta a dormir (%s)", mpu_log);
        CHECK((Sensors_AvailableMask() & SENSOR_MPU6500) == 0, "mpu: indisponivel apos falha de leitura");
        mpu_fail_read = 0;
        mpu6500_available = false; mpu_fail_init = 1; mpu_clear_log(); Sensors_ReadOne(MPU_IDX);
        CHECK(strcmp(mpu_log, "I") == 0, "mpu: sem sensor so tenta o Init (%s)", mpu_log);
        mpu_fail_init = 0;
    }

    printf("verificacoes: %ld, falhas: %ld\n", checks, fails);
    return fails ? 1 : 0;
}
