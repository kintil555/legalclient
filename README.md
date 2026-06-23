# Bedrock Utility Client

Client utilitas legal untuk Minecraft Bedrock Edition, dibangun di atas
C++20 + ImGui + DirectX 11, dengan arsitektur modular event-driven.

## Fitur yang dibangun

| Modul | Kategori | Fungsi |
|---|---|---|
| Menu (Right Shift) | Core/UI | Buka/tutup menu klien |
| Auto Sprint | Utility | Menahan tombol sprint otomatis selama jalan ke depan & hunger cukup |
| Keystrokes HUD | HUD | Overlay WASD + mouse button untuk streaming/rekaman |
| Auto Jump on Damage | Utility | Accessibility: lompat otomatis sekali saat kena damage serangan, dengan cooldown agar tidak bisa di-spam jadi bhop |
| Player ESP | Visual | Outline kotak di sekitar player lain (occlusion-aware via world-to-screen + depth check), tanpa indikator hit/reach |

## Fitur yang TIDAK dibangun (dan kenapa)

Permintaan awal menyertakan **hitbox berwarna merah ketika target bisa di-hit**
(reach/hit-chance indicator). Ini ditolak karena termasuk kategori combat
assist/aim-assist indicator — memberi informasi "kapan menyerang" adalah
keuntungan kompetitif yang tidak adil, terlepas dari apakah server tertentu
mengizinkannya. Sebagai gantinya, `PlayerESP` hanya menampilkan posisi visual
dan jarak (opsional), tanpa logika reach sama sekali.

## Integrasi

1. **Hooking render loop**: gunakan Kiero atau MinHook untuk hook
   `IDXGISwapChain::Present`, lalu panggil `Client::Core::renderClient()`
   setelah `ImGui::NewFrame()`.
2. **Hooking input**: hook `WndProc` (atau gunakan raw input), dan untuk
   setiap `WM_KEYDOWN`/`WM_KEYUP`, dispatch ke
   `Client::Events::EventBus::get().key.dispatch({vKey, isDown})`.
3. **Damage event**: hook fungsi internal game yang menangani damage pada
   local player (lokasi spesifik tergantung versi game), lalu dispatch
   `Client::Events::EventBus::get().damage.dispatch({amount, cause})`.
4. **Tick loop**: panggil `Events::EventBus::get().tick.dispatch({})` pada
   setiap game tick (biasanya hook pada fungsi update entity lokal).
5. Panggil `Client::Core::initializeClient()` sekali saat DLL attach.

## Catatan kompatibilitas

- Semua offset di `SDK/` adalah **placeholder**. Karena Bedrock Edition
  sering update dan tidak punya struktur memory yang stabil antar versi,
  offset harus didapat ulang setiap update besar (gunakan dumper khusus
  versi game yang dipakai).
- `WorldToScreen` mengasumsikan matrix row-major; sesuaikan dengan layout
  matrix view-projection yang sebenarnya dari game (bisa column-major,
  tergantung hasil reverse engineering).
- Build ini ditargetkan untuk Windows (Win32 API + DirectX 11). Tidak
  mendukung platform mobile/console dari Bedrock.

## CI / GitHub Actions

Workflow di `.github/workflows/build.yml` otomatis jalan setiap push/PR ke
`main`. Yang dilakukan:

1. Checkout repo
2. Bootstrap & cache **vcpkg**, install `nlohmann-json` + `minhook` lewat
   manifest (`vcpkg.json`)
3. Clone **ImGui** versi pinned (`v1.90.9`) ke `third_party/imgui` (tidak
   di-commit ke repo, lihat `.gitignore`)
4. Configure + build dengan MSVC (Visual Studio 17 2022, x64, Release)
5. Upload hasil `.dll` sebagai build artifact (bisa diunduh dari tab
   **Actions** di GitHub, retensi 14 hari)

Untuk build lokal di Windows, jalankan langkah yang sama secara manual
(vcpkg install, clone imgui ke `third_party/imgui`, lalu `cmake` + build)
karena tidak ada server CI yang dipakai untuk distribusi binary publik.

## Potensi pitfall

- **AutoJumpOnDamage**: pastikan cooldown tidak di-set terlalu rendah,
  karena itu bisa mulai terlihat seperti auto-bhop. Default 10 tick (~0.5s)
  sudah aman.
- **PlayerESP**: jika world-to-screen salah orientasi, box akan muncul di
  posisi yang salah — selalu validasi dengan kasus sederhana (berdiri di
  depan player lain, cek box-nya pas).
- **EventBus**: dispatch dari thread render vs thread game harus disinkron
  (gunakan critical section/mutex saat dispatch jika hook berjalan di
  thread berbeda), supaya tidak race condition saat akses module list.
