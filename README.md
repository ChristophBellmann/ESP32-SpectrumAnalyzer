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


- when the main frequency does not change, but other freuqeuncys start apperaring
  - emerging higher frequencies could eman something is sliding
  - emerging lower frequencies could mean the motor is running lumpy, bearing failure imminent?
