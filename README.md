# ESP32-Audioworks

Using Audio Input on the ESP32 to figure out whats going on in the surrounding

## Loop

This records a small audio sample and plays it back

## Analyze 

Do some analysis of the sample

### Get dominant frequency

- Detect a frequency which is the loudest in the sample
- Serial output the frequency in Hz

### Detect change in Freqeuncy

short period analysis

- save the Frequency of each of the last 10 samples
- Serial output tendency of speeding up/ down

### Detect anomalies, assuming a running water pump

Long period analysis, over the last 100 samples?

- when the main frequency does not change, but other freuqeuncys start apperaring
  - emerging higher frequencies could eman something is sliding
  - emerging lower frequencies could mean the motor is running lumpy, bearing failure imminent?