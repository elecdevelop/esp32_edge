# ESP32 Edge

The goal is to have a standalone edge-AI device based on ESP32-S3

An ESP32-S3 supermini module is connected to this machine via USB.

This USB connections will be used to feed test data, and received data from the ESP32-S3 board in realtime manner as well.

## Step 1:
As  it is demonstrated in the docs/esp32s3_pdm_noise_cancellation.md the goal is to received data from two digital PDM mics, and perform a variety of mathematical (such as spectral subtraction and LMS adaptive filter) conversions with a cognitive analysis on it. One application would be a AI equipped de-noising setup.

To nail this, for this first step of the project, I would like to have a firmware structure in which data from two channels  of PDM mics --one for the voice and and one for the ambient noise profile -- being acquired in realtime manner, then a 32-bit precision FFT on them, which will feed a proven spectral subtraction de-noising algorithm to be perform on them. The outcome of this de-noising algorithm, would be sent to an iFFT block, to be send out as a de-noised signal. 

To perform a realtime assess/tweak on the de-noising algorithm output, an AI model is considered. This AI model is supposed to act like a human user to access the success of the de-noising algorithm, and then perform a variety of tweaks on the de-noising algorithm settings to make its output as perfect as possible.

For this first step, there is no AI model involved. also, the digital PDM mics are not connected to the ESP32-S3 supermini board. Hence, I would like the firmware to perform on some test data; on the noise profile mic, we have a 1.7KHz square signal, while on the voice mic we are supposed have a mix of a 1Khz sine wave and the already mentioned 1.7KHz square signal. The sound sampling would be 16-bit and 16ks/s .

A five second long sample of the output will be sent to the computer via USB storage protocol, to be saved as a wave file. 

