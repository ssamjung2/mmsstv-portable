4.2.5 Amiga Video Transceiver
AVT modes were originally intended for SSTV operations with Amiga computers.
AVT author Ben B. Williams, AA7AS developed a dedicated interface and software
which was produced by AEA (Advanced Electronic Applications Inc.). Although
the creator claimed that this system was a revolution in SSTV transmission, these
modes did not gain popularity like other modes. The AVT modes are practically
not in use today.
A reason for this could be the fact that the manufacturer wanted to keep the
image scan parameters of the system, secret. However, by intercepting signals and
reverse engineering, the parameters of the AVT modes were implemented in other
devices by the SSTV community. This was done without the additional software
tools that made the AVT unique.
The AVT system contains four line sequential RGB modes and one B&W. The
scan-lines have no gaps between color components and a really unusual thing is that;
the modes do not use any horizontal sync. Another unusual feature is the mandatory
function of vertical synchronization, that is sent as a digital header before the image
transfer begins.
The AVT family contains 5 modes and each of them has the following four options:
1. Default variant is the same as conventional SSTV modes, but does not have any
line syncs.
2. Narrowband variant uses shorter band for video signals from 1700 Hz for black
to 2100 Hz for white.
3. QRM variant, that uses picture interlacing just like in analog television.
4. The combination of the QRM and narrowband variant.
The fastest mode is the AVT 24 with 120 lines and it is transferred for 31 seconds.
The next mode is AVT 90 with a resolution of 256×240 and an image quality slightly
worse than in the Martin M1. ATV 90 sends each color component in 125.0 ms, thus
the speed is 2048 pixels per second (in binary notation this gives a nice rounded
number). The other two modes have somewhat atypical resolutions in comparison
with other SSTV modes, but these resolutions are normal system resolutions on
Amiga computers. It is AVT 94 with 320×200 and AVT 188 with the same line
speed, but twice the scan-lines – 320×400. The image is displayed in an aspect ratio
of 4 : 3 in both cases.
For some SSTV systems/scan-converters, the detection of vertical sync is a must.
So, the VIS code is repeated three times for accurate reception. VIS is necessary for
image reception when no line sync is sent and later synchronization is not possible.
The original AVT software however, does not need to receive VIS, but relies more
on the digital header.
After a series of VIS code, there is a digital header (see fig. 4.7), which contains
synchronization data. It is a sequence of 32 frames of 16 bits. Each frame contains
only 8 bits of information, but it is sent twice – first in normal form and second
inverted. Normal and inverted parts can be compared for error detection. Each
frame starts with a 1900Hz pulse while data modulation uses 1600 Hz for the representation of logical zeros and 2200 Hz for logical ones. Narrow-band variants use
1700 Hz for zeros and 2100 Hz for ones. Both variants use a modulation speed of
exactly 2048/20 = 102.4 Bd, so the data pulse has a length of 9.766 ms.
The first three bit of each 8bit word identifies the mode:
⊳ 010 – AVT 24,
⊳ 011 – AVT 94, AVT 188, AVT 125 BW,
⊳ 101 – AVT 90.
The last five bits are used as a count down before image transmission. Actually
these five bits are important for an accurate set of image initiation and synchronization. They vary between all 32 binary combinations during transmission. At least
one binary code must be properly detected. At the beginning, all bits are in 0 states
with 1 in inverted parts. When the countdown starts, all five-bit sequences run (e.g.
for AVT 24):
010 00000 101 11111
010 00001 101 11110
010 00010 101 11101
…
010 11101 101 00010
010 11110 101 00001
010 11111 101 00000
When the count down gets to zero, the image scan-lines are sent. AVT reception depends on the first eight seconds of synchronization,for some implementations
without the ability to synchronize later. Although the AVT modes are quite reliable, noise could cause a loss of the whole image. Sometimes it is not possible to
receive a digital header due to interference, even if the interference later disappears.
However, the original AVT software was capable of image reconstruction in this case.
Because the image data is completely synchronous, the data simply has to be shifted
in memory until the RGB data is aligned correctly, and then the image comes out
perfectly. Again, the AVT system provided means to hot reconfigure the data after
reception. So reception without/after sync header worked fine.
The earlier listed options for each mode can improve its performance. The first
is the narrow-band transmission which uses a 400 Hz band from 1700 Hz (black) to
2100 Hz (white). With an appropriate filter, the resistance to interference can be
improved with minimal loss of image quality. For instance; the 400 Hz wide CW
filter can be used with a variable IF shift.
The second option is the “QRM mode”, where an entire image is sent interlaced.
Within the first half of image transmission time, half of the scan lines (every odd
one) is sent. Then the scan loops back to the beginning and sends the remaining
half lines (even lines). The fact that some of the disturbed lines of the first field is
interlaced with fine lines received from the second will definitely improve the overall
subjective impression of image quality. The original AVT software also contains
tools for handy image improvement – it is possible to select distorted lines and
the program will reconstruct them by averaging neighborhood lines. It is also is
possible to shift the second field horizontally independently of the first field. This
allows you to compensate if there is a significant multi-path delay in regard to the
two fields.
In ATV implementations, the system can work well without this interactive tools.
But in practice, especially on shortwaves where conditions change quickly; the second
field could be phase-shifted and this causes the notable “toothy” edge of the picture.
The QRM option can be combined with the narrow-band mode.
Mode Transfer
Resolution
Color Scan line (ms) Speed
name time sequence Sync R G B (lpm)
AVT 24 31 s 128×120 R–G–B — 62.5 62.5 62.5 960.000
AVT 90 98 s 256×240 R–G–B — 125.0 125.0 125.0 480.000
AVT 94 102 s 320×200 R–G–B — 156.25 156.25 156.25 384.000
AVT 188 196 s 320×400 R–G–B — 156.25 156.25 156.25 384.000
AVT 125 BW 133 s 320×400 Y — 312.5 192.00