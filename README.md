# Konsol Game Mini Arduino Nano (Tetris & Snake)

Konsol game retro berbasis **Arduino Nano** dengan layar **LED matrix 8×32 (4in1 modul MAX7219)**. Dikendalikan memakai **Funduino Joystick Shield** (joystick analog dan tombol A–F), dilengkapi **active buzzer** untuk efek suara dan musik latar.

## Fitur

- Dua game dalam satu perangkat: **Tetris** dan **Snake**, dapat berpindah dengan satu tombol
- Kontrol umum: pause/resume, restart, dan sound on/off
- Animasi startup ("TETRIS") dan animasi game over ("GAME OVER")
- **Tetris**: 7 jenis balok, rotasi, hard drop, soft drop lewat joystick, dan fitur *hold*
- **Snake**: kecepatan bertambah setiap makan apel, mode *boost*, apel berkedip
- Rendering efisien dengan buffer ganda: hanya baris yang berubah yang dikirim ke MAX7219
- Musik latar *non-blocking* berbasis `millis()`, sehingga game tidak terhenti

## Komponen

| No | Komponen | Jumlah | Fungsi |
|----|----------|--------|--------|
| 1 | Arduino Nano (ATmega328P) | 1 | Mikrokontroler utama |
| 2 | Funduino Joystick Shield | 1 | Joystick analog, tombol joystick (K), dan tombol A–F |
| 3 | Modul LED Matrix 8×8 MAX7219 | 4in1 | Layar 8×32 (dirangkai *cascade*) |
| 4 | Active buzzer | 1 | Efek suara dan musik |
| 5 | Kabel jumper | Secukupnya | Penghubung antarkomponen |
| 6 | Catu daya 5V / kabel USB | 1 | Sumber daya |

> Shield Funduino berukuran seperti Arduino Uno. Untuk Arduino Nano, gunakan adaptor/*expansion board* Nano atau hubungkan dengan kabel jumper.

## Perangkat Lunak

- [Arduino IDE](https://www.arduino.cc/en/software)
- Library **LedControl**

## Pemetaan Pin

### Layar dan buzzer

| Fungsi | Pin Arduino |
|--------|-------------|
| MAX7219 DIN | D11 |
| MAX7219 CLK | D13 |
| MAX7219 CS | D10 |
| Buzzer | D9 |
| Seed acak | A2 (dibiarkan mengambang) |

Modul MAX7219 juga memerlukan sambungan **VCC (5V)** dan **GND**.

### Input (Funduino Joystick Shield)

| Input | Pin | Fungsi |
|-------|-----|--------|
| Joystick X / Y | A0 / A1 | Gerak balok, soft drop (Tetris); arah ular (Snake) |
| Tombol K (tekan joystick) | D8 | *Hold* balok (Tetris); mulai ulang setelah game over |
| Tombol A | D2 | Hard drop (Tetris); restart setelah game over (Snake) |
| Tombol B | D3 | Rotasi (Tetris); boost kecepatan (Snake) |
| Tombol C | D4 | Restart game yang sedang aktif |
| Tombol D | D5 | Pause / Resume |
| Tombol E | D6 | Ganti game (Tetris ↔ Snake) |
| Tombol F | D7 | Suara on/off |


## Cara Menjalankan

1. Rakit rangkaian sesuai tabel pemetaan pin di atas.
2. Instal library **LedControl** di Arduino IDE.
3. Buka file `.ino` (pastikan nama folder sama dengan nama file `.ino`).
4. Pilih board **Arduino Nano** dan processor **ATmega328P** (coba *Old Bootloader* jika upload gagal), lalu pilih port yang sesuai.
5. Klik **Upload**.

## Cara Bermain

### Kontrol umum (kedua game)

| Tombol | Fungsi |
|--------|--------|
| D | Pause / Resume |
| C | Restart game |
| E | Ganti game |
| F | Suara on/off |

### Tetris

| Kontrol | Fungsi |
|---------|--------|
| Joystick kiri / kanan | Geser balok |
| Joystick bawah | Mempercepat jatuh (soft drop) |
| Tombol B | Putar balok |
| Tombol A | Hard drop (langsung jatuh ke dasar) |
| Tekan joystick (K) | *Hold*: simpan balok atau tukar dengan balok yang disimpan (sekali per balok) |

Baris yang terisi penuh akan terhapus. Game over terjadi saat balok menumpuk hingga baris paling atas. Tekan joystick untuk memulai ulang.

### Snake

| Kontrol | Fungsi |
|---------|--------|
| Joystick | Menentukan arah ular (tidak bisa berbalik 180°) |
| Tombol B (tahan) | Boost kecepatan |
| Tombol A atau joystick (K) | Mulai ulang setelah game over |

Makan apel untuk memanjangkan ular dan menambah kecepatan. Menabrak dinding atau tubuh sendiri berarti game over. Jika seluruh layar terisi, permainan dianggap menang dan diulang dari awal.

## Cara Kerja Singkat

1. **Inisialisasi**: pin diatur, modul MAX7219 dinyalakan dan dikosongkan, lalu animasi startup diputar.
2. **Tampilan**: koordinat (x, y) dipetakan ke modul dan baris MAX7219 lewat `bufPlot()` dan `flushBuf()`. Layar disegarkan sekitar 30 kali per detik.
3. **Loop utama**: membaca tombol F, E, C, dan D secara berurutan, lalu menjalankan musik dan logika game yang aktif.
4. **Tetris**: papan disimpan sebagai array `field[32]` (satu byte per baris), dengan pengecekan tabrakan, penguncian balok, dan penghapusan baris penuh.
5. **Snake**: tubuh ular disimpan sebagai array indeks sel (`y*8 + x`), digerakkan per interval waktu, dengan pengecekan tabrakan dan makan apel.

## Struktur Repositori

```
tetrisANDsnakes/
├── tetrisANDsnakes.ino
└── README.md
```

