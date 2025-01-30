# ESP32-SpectrumAnalyzer

Using the analog input on a ESP32 to figure out whats going on in the surrounding

Steps are:
* Internet connection over wifi
* Continuos high-speed data aquisition
* Fast Fourier Transformation
* FFT baseline difference detection
* Send frequency data to a server for visualisation

Signal spec:
|capture speed [sps]|buffer size [bit]|resolution [bit]|
|--------|--------|--------|
|441000|1024|12|  

## Loop

This records a small wave sample and plays it back

Playback in the browser:
* Audio file (sound.mp3)
* Visualisation (brightness box)

Using an appropriate output device:
* For audio: Amplifier/ Speaker
* For visual spectrum: LED

## Analyze 

Do some analysis of the sample
Frequnecy range: 1 Hz .. 44.1 kHz

### Get main frequency

- Detect the frequency with the highest amplitude in the sample
- Serial output the frequency in Hz

### Detect change in Freqeuncy

short period analysis

- save the frequency of each of the last 10 samples
- Serial output tendency of speeding up/ down

### Detect anomalies, assuming a running water pump

Long period analysis, over the last 50 samples

- when the main frequency does not change, but other freuqeuncys start apperaring
  - emerging higher frequencies could mean something is sliding or started to get loose
  - emerging lower frequencies could mean the motor is running lumpy, bearing failure imminent?

---

# Structure/ Project Overview

## Files

- **config.h**  
  Contains configuration values such as `WIFI_SSID`, `WIFI_PASS`, `SAMPLE_RATE`, `FFT_SIZE`, etc.

- **adc_fft.h / adc_fft.c**  
  - **Functions:**  
    - `configure_adc_continuous()`  
      Configures the ADC in continuous mode.  
    - `collect_adc_continuous_data()`  
      Continuously reads ADC data and performs FFT calculations.
  - **Global Variables:**  
    - `adc_handle`  
      ADC handle for continuous mode.  
    - `main_frequency`, `max_magnitude`  
      Results of the FFT calculation (main frequency and maximum amplitude).

- **wav.h / wav.c**  
  - **Functions:**  
    - `save_adc_data()`  
      Stores 50 ADC data blocks (buffers) for later processing.  
    - `wav_download_handler()`  
      Generates a WAV file from the stored ADC data and sends it as a download to the client.  
    - `generate_wav_header()`  
      Creates the WAV file header based on configuration values and data size.

- **wifi.h / wifi.c**  
  - **Functions:**  
    - `wifi_init_sta()`  
      Initializes the Wi-Fi connection.  
    - `start_webserver()`  
      Starts the HTTP server and registers URI handlers.  
    - `main_frequency_handler()`  
      Outputs the main frequency and magnitude as an HTML page.  
    - **External Function:**  
      `wav_download_handler()`  
      Included to generate and deliver the WAV file upon request.

## How It Works

1. **Wi-Fi Setup:**  
   - `wifi_init_sta()` establishes the Wi-Fi connection.
   - `start_webserver()` starts the HTTP server.

2. **ADC Data Processing:**  
   - `configure_adc_continuous()` sets up the ADC in continuous mode.
   - `collect_adc_continuous_data()` reads ADC data, performs an FFT, and stores the main frequency and magnitude in global variables.

3. **WAV Generation and Download:**  
   - When the user clicks the download button, `wav_download_handler()` is called.
   - This function saves the ADC data in buffers, creates a WAV file, and sends it as a download to the client.

  # **Calculations for Time per Bar in Frequency Trend Chart**

# Calculations for Time per Bar in Frequency Trend Chart

## 1. Understanding the Backend Configuration

### a. Sampling Rate (`SAMPLE_RATE`)
The sampling rate determines how many samples are captured per second. As defined in `config.h` and utilized in `adc_fft.c`:
\[
\text{SAMPLE\_RATE} = 8000 \, \text{Hz}
\]

### b. FFT Size (`FFT_SIZE`)
The number of samples processed in each Fast Fourier Transform (FFT) operation, defined in `config.h` and used in both `adc_fft.c` and `fastdetect.c`:
\[
\text{FFT\_SIZE} = 1024
\]

### c. Time per FFT Operation (`T_{\text{FFT}}`)
Each FFT processes a block of samples. The time duration each FFT represents is calculated as:
\[
T_{\text{FFT}} = \frac{\text{FFT\_SIZE}}{\text{SAMPLE\_RATE}} = \frac{1024}{8000} = 0.128 \, \text{seconds} = 128 \, \text{ms}
\]

## 2. Frequency Measurements Collection

### a. Frequency Measurements per Chunk
In `fastdetect.c`, each chunk aggregates up to 4 frequency measurements. This is evident from the loop:
`for (int i = 0; i < 4; i++) { float f = get_recent_freq(i); /* ... */ }`
\[
\text{Measurements per Chunk} = 4
\]

### b. Time per Chunk (`T_{\text{Chunk}}`)
Each chunk aggregates 4 FFT-based frequency measurements. Therefore, the time each chunk represents is:
\[
T_{\text{Chunk}} = \text{Measurements per Chunk} \times T_{\text{FFT}} = 4 \times 128 \, \text{ms} = 512 \, \text{ms}
\]

## 3. Number of Chunks (`NUM_CHUNKS`)
The total number of chunks determines how many bars are displayed on the frontend, as defined in `fastdetect.c`:
\[
\text{NUM\_CHUNKS} = 40
\]

## 4. Time per Bar (`T_{\text{Bar}}`)
Each bar in the trend chart represents one chunk. Therefore, the time each bar represents is:
\[
T_{\text{Bar}} = T_{\text{Chunk}} = 512 \, \text{ms}
\]

## 5. Total Time Displayed on Chart (`T_{\text{Total}}`)
The total duration represented by all bars in the trend chart is:
\[
T_{\text{Total}} = \text{NUM\_CHUNKS} \times T_{\text{Bar}} = 40 \times 512 \, \text{ms} = 20,480 \, \text{ms} \approx 20.48 \, \text{seconds}
\]

## 6. Time per Step (`T_{\text{Step}}`)
Given that each step aggregates 4 bars, the time each step represents is:
\[
T_{\text{Step}} = \text{Bars per Step} \times T_{\text{Bar}} = 4 \times 512 \, \text{ms} = 2,048 \, \text{ms} \approx 2.048 \, \text{seconds}
\]

## 7. Summary of Calculations

| Parameter               | Symbol                  | Value                |
|-------------------------|-------------------------|----------------------|
| Sampling Rate           | \(\text{SAMPLE\_RATE}\) | \(8000 \, \text{Hz}\)|
| FFT Size                | \(\text{FFT\_SIZE}\)    | \(1024\)             |
| Time per FFT            | \(T_{\text{FFT}}\)      | \(128 \, \text{ms}\) |
| Measurements per Chunk  | \(\text{M}_{\text{chunk}}\) | \(4\)               |
| Time per Chunk          | \(T_{\text{Chunk}}\)    | \(512 \, \text{ms}\) |
| Number of Chunks        | \(\text{NUM\_CHUNKS}\)  | \(40\)               |
| Time per Bar            | \(T_{\text{Bar}}\)      | \(512 \, \text{ms}\) |
| Total Time on Chart     | \(T_{\text{Total}}\)    | \(20.48 \, \text{seconds}\) |
| Bars per Step           | \(\text{BARS\_PER\_STEP}\) | \(4\)               |
| Time per Step           | \(T_{\text{Step}}\)     | \(2.048 \, \text{seconds}\) |

## 8. Visual Representation on Frontend

- **X-Axis Labels:** Displayed every 4 bars, representing \(2.048 \, \text{seconds}\).
- **Legend:** Indicates that each bar represents \(512 \, \text{ms}\).

## 9. Alignment with Frontend Configuration

Ensure that the frontend (`fastdetect.html`) reflects these calculations accurately:

- **Constants in Frontend JavaScript:**
  ```javascript
  const NUM_CHUNKS = 40; // Must match backend NUM_CHUNKS
  const TIME_PER_BAR_MS = 512; // Calculated as 4 * 128ms
  const BARS_PER_STEP = 4; // Display time every 4 bars
