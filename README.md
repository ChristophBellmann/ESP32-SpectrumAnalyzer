# ESP32-SpectrumAnalyzer

Using Analog Input on the ESP32 to figure out whats going on in the surrounding

Specs:
|capture speed [sps]|sample lenght|bitrate|
|--------|--------|--------|
|441000|441|12|  

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

### Get dominant frequency

- Detect the frequency with the highest amplitude in the sample
- Serial output the frequency in Hz

### Detect change in Freqeuncy

short period analysis

- save the frequency of each of the last 10 samples
- Serial output tendency of speeding up/ down

### Detect anomalies, assuming a running water pump

Long period analysis, over the last 100 samples?

- when the main frequency does not change, but other freuqeuncys start apperaring
  - emerging higher frequencies could eman something is sliding
  - emerging lower frequencies could mean the motor is running lumpy, bearing failure imminent?
