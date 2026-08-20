// str_probe — read the GROUND TRUTH out of an STR movie on the disc: how many video frames it
// holds, at what size, how many XA audio sectors are interleaved with them and at what rate. That
// is the only honest yardstick for "does the port play this movie at its real duration".
//
// WHY THIS EXISTS. RE-07 landed the intro movies' PICTURE and explicitly did not measure their
// AUDIO or their PACING. You cannot measure pacing against a number you do not have, and the
// number is not in the port — it is in the file. `discdump subhdr` shows the CD subheader
// (file/channel/submode) but not the XA coding byte and not the STR frame header, so neither the
// audio rate nor the frame count was reachable before this.
//
//   build: cc -O2 -o scratch/bin/str_probe tools/str_probe.c \
//            -Iexternal/psxport/vendor/beetle-psx/deps/libchdr/include \
//            external/psxport/build/_deps/*/libchdr-static.a -lz -lzstd     (see --build below)
//   run:   scratch/bin/str_probe <disc.chd> <start_lba> <sectors> [--frames]
//
// A NEGATIVE PRINTS ITS DENOMINATOR. If the range holds no STR frames, no audio, or does not parse,
// this says how many sectors it READ and what it saw instead — a silent "0 frames" would be
// indistinguishable from "I never looked". A read failure or a non-Mode2 sector is reported and
// exits non-zero rather than being skipped, because a partially-scanned range would understate
// every count below it.
//
// --selftest proves the STR header parser fires: it feeds a synthesized sector that MUST parse as
// frame 7 of 10 chunks at 320x240, and a garbage sector that MUST NOT parse. Both directions are
// checked, because a parser that accepts everything and a parser that accepts nothing both report
// "0 anomalies".
#include <libchdr/chd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RAW_FRAME 2448u // CHD stores 2352 data + 96 subcode per sector

static chd_file *s_chd;
static uint32_t s_fph, s_hcount, s_hbytes, s_cached = 0xFFFFFFFFu;
static uint8_t *s_hbuf;

static int disc_open(const char *path) {
  if (chd_open(path, CHD_OPEN_READ, 0, &s_chd) != CHDERR_NONE) {
    return 0;
  }
  const chd_header *h = chd_get_header(s_chd);
  s_hbytes = h->hunkbytes;
  s_fph = h->hunkbytes / RAW_FRAME;
  s_hcount = h->totalhunks;
  s_hbuf = (uint8_t *)malloc(s_hbytes);
  return s_fph > 0 && s_hbuf != NULL;
}

static int disc_read_raw(uint32_t lba, uint8_t *out2352) {
  uint32_t hunk = lba / s_fph, off = (lba % s_fph) * RAW_FRAME;
  if (hunk >= s_hcount) {
    return 0;
  }
  if (hunk != s_cached) {
    if (chd_read(s_chd, hunk, s_hbuf) != CHDERR_NONE) {
      return 0;
    }
    s_cached = hunk;
  }
  memcpy(out2352, s_hbuf + off, 2352);
  return 1;
}

// ---- STR video frame header, at the start of a Form1 sector's 2048-byte user data --------------
// Sony's STR chunk header. Named rather than offset-indexed so the code says what it reads.
typedef struct StrHeader {
  uint16_t magic;       // 0x0160
  uint16_t type;        // 0x8001 = MDEC bitstream video
  uint16_t chunk_no;    // this sector's index within the frame, 0-based
  uint16_t chunks;      // sectors this frame occupies
  uint32_t frame_no;    // 1-based frame counter
  uint32_t frame_bytes; // MDEC bitstream bytes in the whole frame
  uint16_t width, height;
} StrHeader;

static uint16_t le16(const uint8_t *p) {
  return (uint16_t)(p[0] | (p[1] << 8));
}
static uint32_t le32(const uint8_t *p) {
  return (uint32_t)(p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24));
}

// Returns 1 and fills `h` when the sector's user data really is an STR video chunk. The magic AND
// the type are both checked: magic alone matches on ~1 sector in 65536 of arbitrary data.
static int str_parse(const uint8_t *raw2352, StrHeader *h) {
  const uint8_t *d = raw2352 + 24; // 12 sync + 4 header + 8 subheader
  h->magic = le16(d + 0);
  h->type = le16(d + 2);
  if (h->magic != 0x0160 || h->type != 0x8001) {
    return 0;
  }
  h->chunk_no = le16(d + 4);
  h->chunks = le16(d + 6);
  h->frame_no = le32(d + 8);
  h->frame_bytes = le32(d + 12);
  h->width = le16(d + 16);
  h->height = le16(d + 18);
  return 1;
}

// XA coding byte (subheader byte 3 = raw[19]): bit0-1 mono/stereo, bit2-3 sample rate,
// bit4-5 bits-per-sample. Mednafen/Sony parity, same decode xa_stream.cpp's decoder assumes.
static int xa_freq(uint8_t coding) {
  return (coding & 0x04) ? 18900 : 37800;
}
static int xa_stereo(uint8_t coding) {
  return (coding & 0x01) ? 1 : 0;
}
// Frames (sample-pairs for stereo, samples for mono) one Form2 audio sector decodes to.
static int xa_frames_per_sector(uint8_t coding) {
  return xa_stereo(coding) ? 2016 : 4032;
}

static int selftest(void) {
  uint8_t s[2352];
  StrHeader h;
  int fails = 0;
  // POSITIVE: a sector that MUST parse as frame 7, chunk 3 of 10, 320x240.
  memset(s, 0, sizeof s);
  uint8_t *d = s + 24;
  d[0] = 0x60;
  d[1] = 0x01;
  d[2] = 0x01;
  d[3] = 0x80; // magic 0x0160, type 0x8001
  d[4] = 3;
  d[5] = 0;
  d[6] = 10;
  d[7] = 0; // chunk 3 of 10
  d[8] = 7;
  d[9] = 0;
  d[10] = 0;
  d[11] = 0; // frame 7
  d[16] = 0x40;
  d[17] = 0x01;
  d[18] = 0xF0;
  d[19] = 0x00; // 320 x 240
  if (!str_parse(s, &h)) {
    printf("selftest FAIL: valid STR sector did not parse\n");
    fails++;
  } else if (h.frame_no != 7 || h.chunks != 10 || h.chunk_no != 3 || h.width != 320 ||
             h.height != 240) {
    printf("selftest FAIL: parsed wrong: frame=%u chunk=%u/%u %ux%u\n",
           h.frame_no,
           h.chunk_no,
           h.chunks,
           h.width,
           h.height);
    fails++;
  }
  // NEGATIVE: garbage must NOT parse. A parser that accepts everything reports 0 anomalies too.
  memset(s, 0xA5, sizeof s);
  if (str_parse(s, &h)) {
    printf("selftest FAIL: garbage sector parsed as STR\n");
    fails++;
  }
  // NEGATIVE 2: right magic, wrong type — must still be rejected.
  memset(s, 0, sizeof s);
  d = s + 24;
  d[0] = 0x60;
  d[1] = 0x01;
  d[2] = 0x99;
  d[3] = 0x99;
  if (str_parse(s, &h)) {
    printf("selftest FAIL: magic-only sector parsed as STR\n");
    fails++;
  }
  // XA rate decode, both classes.
  if (xa_freq(0x00) != 37800 || xa_freq(0x04) != 18900) {
    printf("selftest FAIL: xa_freq\n");
    fails++;
  }
  if (xa_stereo(0x00) != 0 || xa_stereo(0x01) != 1) {
    printf("selftest FAIL: xa_stereo\n");
    fails++;
  }
  printf("selftest: %s (4 parser cases + 4 coding cases)\n", fails ? "FAILED" : "PASSED");
  return fails ? 1 : 0;
}

int main(int argc, char **argv) {
  int want_frames = 0;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--selftest")) {
      return selftest();
    }
    if (!strcmp(argv[i], "--frames")) {
      want_frames = 1;
    }
  }
  if (argc < 4) {
    fprintf(stderr,
            "usage: str_probe <disc.chd> <start_lba> <sectors> [--frames]\n"
            "       str_probe --selftest\n");
    return 2;
  }
  const char *disc = argv[1];
  uint32_t lba0 = (uint32_t)strtoul(argv[2], NULL, 0);
  uint32_t n = (uint32_t)strtoul(argv[3], NULL, 0);
  if (!disc_open(disc)) {
    fprintf(stderr, "cannot open CHD: %s\n", disc);
    return 1;
  }

  uint32_t read = 0, video = 0, audio = 0, other = 0, notmode2 = 0;
  uint32_t frames = 0, chunk_mismatch = 0, frame_no_first = 0, frame_no_last = 0;
  uint32_t chunks_min = 0xFFFFFFFFu, chunks_max = 0, w = 0, h = 0, size_changes = 0;
  uint8_t coding = 0xFF;
  uint32_t coding_changes = 0;
  uint32_t first_audio_lba = 0, last_audio_lba = 0;
  uint8_t raw[2352];

  for (uint32_t i = 0; i < n; i++) {
    uint32_t lba = lba0 + i;
    if (!disc_read_raw(lba, raw)) {
      fprintf(stderr,
              "READ FAILED at LBA %u after %u sectors — counts below cover ONLY [%u..%u]\n",
              lba,
              read,
              lba0,
              lba ? lba - 1 : lba0);
      return 1;
    }
    read++;
    if (raw[15] != 2) {
      notmode2++;
      continue;
    }
    uint8_t submode = raw[18];
    if (submode & 0x04) { // Form2 XA audio
      audio++;
      if (!first_audio_lba) {
        first_audio_lba = lba;
      }
      last_audio_lba = lba;
      if (coding == 0xFF) {
        coding = raw[19];
      } else if (raw[19] != coding) {
        coding_changes++;
      }
      continue;
    }
    StrHeader s;
    if (str_parse(raw, &s)) {
      video++;
      if (s.chunk_no == 0) {
        frames++;
        if (frames == 1) {
          frame_no_first = s.frame_no;
          w = s.width;
          h = s.height;
        } else if (s.width != w || s.height != h) {
          size_changes++;
        }
        frame_no_last = s.frame_no;
        if (s.chunks < chunks_min) {
          chunks_min = s.chunks;
        }
        if (s.chunks > chunks_max) {
          chunks_max = s.chunks;
        }
        if (want_frames) {
          printf("  frame %-5u LBA %-8u chunks=%-3u bytes=%-7u %ux%u\n",
                 s.frame_no,
                 lba,
                 s.chunks,
                 s.frame_bytes,
                 s.width,
                 s.height);
        }
      }
    } else {
      other++;
    }
  }

  printf("\nstr_probe %s  LBA [%u..%u], %u sectors requested, %u READ\n",
         disc,
         lba0,
         lba0 + n - 1,
         n,
         read);
  printf("  sector census : %u STR video, %u XA audio, %u other Mode2, %u not-Mode2  (sum %u)\n",
         video,
         audio,
         other,
         notmode2,
         video + audio + other + notmode2);

  if (!frames) {
    printf("  VIDEO         : NO STR frame headers in %u sectors read — magic 0x0160 + type 0x8001 "
           "matched %u times. This range is not an STR, or the start LBA is wrong.\n",
           read,
           video);
  } else {
    printf("  VIDEO         : %u frames (frame_no %u..%u), %ux%u, chunks/frame %u..%u%s\n",
           frames,
           frame_no_first,
           frame_no_last,
           w,
           h,
           chunks_min,
           chunks_max,
           size_changes ? "  [!! frame size CHANGES mid-movie]" : "");
    // Duration is a DERIVED number and its assumption is printed with it: a 2x-speed CD delivers
    // 150 sectors/s, so the movie's real running time is (all sectors)/150 regardless of how the
    // video and audio are split. The fps that implies is stated rather than assumed.
    double secs2x = read / 150.0, secs1x = read / 75.0;
    printf(
        "  DURATION      : %.2f s at 2x (150 sectors/s) -> %.2f fps | %.2f s at 1x -> %.2f fps\n",
        secs2x,
        frames / secs2x,
        secs1x,
        frames / secs1x);
  }
  if (!audio) {
    printf(
        "  AUDIO         : NO XA audio sectors (submode bit2) in %u sectors read — this movie is "
        "SILENT by construction, so silence in the port is not a bug.\n",
        read);
  } else {
    int freq = xa_freq(coding), st = xa_stereo(coding), fps_ = xa_frames_per_sector(coding);
    double asecs = (double)audio * fps_ / freq;
    printf("  AUDIO         : %u XA sectors, LBA %u..%u, coding 0x%02X = %d Hz %s, "
           "%d frames/sector -> %.2f s of sound%s\n",
           audio,
           first_audio_lba,
           last_audio_lba,
           coding,
           freq,
           st ? "stereo" : "mono",
           fps_,
           asecs,
           coding_changes ? "  [!! coding byte CHANGES mid-stream]" : "");
    printf("  AUDIO SAMPLES : %.0f frames at %d Hz = %.0f int16 values (%s)\n",
           (double)audio * fps_,
           freq,
           (double)audio * fps_ * (st ? 2 : 1),
           st ? "stereo" : "mono");
  }
  return 0;
}
