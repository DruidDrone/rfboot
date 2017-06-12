# rfboot

Atmega328 Bootloader for wireless (OTA) code uploads, using the CC1101 RF chip.

Included :

- The actual bootloader, intended to be installed to the project's MCU (atmega328), via an
ISP programmer.
- A powerful utility (rftool) to easily create new projects (the directory structure) using rfboot as bootloader.
The same utility used for code upload. Every project is created with unique RF settings and a
preconfigured Makefile. A "make send" does all the job  needed.
- Instructions to assemble a **usb2rf module**, witch allows us to upload code to the target and,
**equally important**, to use a Serial Terminal (like gtkterm) to send and receive text,
analogous to Serial.print()  and Serial.read().

This project's emphasis is on reliability, security and usability. 

- [Installation.](help/Installation.md) Instructions to make the process as straightforward as possible.
- [The first project.](help/The-First-Project.md) Instructions to successfully complete your first Arduino project using a bare atmega328 as target, and rfboot as bootloader.
- [Reliability.](help/Reliability.md) Explains how rfboot does the job, under an inherently unreliable link (RF).
- [Encryption.](help/Encryption.md) Discuses how the firmware is encrypted over the air, and why this is useful.
- [Notes about the License.](help/Notes-about-the-License.md) Some clarifications of the License.
