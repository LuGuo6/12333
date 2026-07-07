/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef _XUANJIE_O1_H
#define _XUANJIE_O1_H

#define MAX_TRACKED_FDS      64
#define MAX_PATH_LEN         256

// ============================================================
// CPU 伪造 — 小米玄戒O1 (8核: 2×Cortex-X925 + 6×Cortex-A725)
// ============================================================

#define FAKE_CPUINFO_SIZE    0xD77

#define FAKE_CPUINFO_CONTENT \
  "processor\t: 0\n" \
  "BogoMIPS\t: 76.80\n" \
  "Features\t: fp asimd evtstrm aes pmull sha1 sha2 crc32 atomics fphp asimdhp cpuid asimdrdm jscvt fcma lrcpc dcpop sha3 sm3 sm4 asimddp sha512 sve asimdfhm dit uscat ilrcpc flagm sb paca pacg dcpodp sve2 sveaes svepmull svebitperm svesha3 svesm4 flagm2 frint svei8mm svebf16 i8mm bf16 dgh bti ecv afp wfxt\n" \
  "CPU implementer\t: 0x41\n" \
  "CPU architecture: 8\n" \
  "CPU variant\t: 0x0\n" \
  "CPU part\t: 0xd80\n" \
  "CPU revision\t: 1\n" \
  "\n" \
  "processor\t: 1\n" \
  "BogoMIPS\t: 76.80\n" \
  "Features\t: fp asimd evtstrm aes pmull sha1 sha2 crc32 atomics fphp asimdhp cpuid asimdrdm jscvt fcma lrcpc dcpop sha3 sm3 sm4 asimddp sha512 sve asimdfhm dit uscat ilrcpc flagm sb paca pacg dcpodp sve2 sveaes svepmull svebitperm svesha3 svesm4 flagm2 frint svei8mm svebf16 i8mm bf16 dgh bti ecv afp wfxt\n" \
  "CPU implementer\t: 0x41\n" \
  "CPU architecture: 8\n" \
  "CPU variant\t: 0x0\n" \
  "CPU part\t: 0xd80\n" \
  "CPU revision\t: 1\n" \
  "\n" \
  "processor\t: 2\n" \
  "BogoMIPS\t: 76.80\n" \
  "Features\t: fp asimd evtstrm aes pmull sha1 sha2 crc32 atomics fphp asimdhp cpuid asimdrdm jscvt fcma lrcpc dcpop sha3 sm3 sm4 asimddp sha512 sve asimdfhm dit uscat ilrcpc flagm sb paca pacg dcpodp sve2 sveaes svepmull svebitperm svesha3 svesm4 flagm2 frint svei8mm svebf16 i8mm bf16 dgh bti ecv afp wfxt\n" \
  "CPU implementer\t: 0x41\n" \
  "CPU architecture: 8\n" \
  "CPU variant\t: 0x0\n" \
  "CPU part\t: 0xd87\n" \
  "CPU revision\t: 1\n" \
  "\n" \
  "processor\t: 3\n" \
  "BogoMIPS\t: 76.80\n" \
  "Features\t: fp asimd evtstrm aes pmull sha1 sha2 crc32 atomics fphp asimdhp cpuid asimdrdm jscvt fcma lrcpc dcpop sha3 sm3 sm4 asimddp sha512 sve asimdfhm dit uscat ilrcpc flagm sb paca pacg dcpodp sve2 sveaes svepmull svebitperm svesha3 svesm4 flagm2 frint svei8mm svebf16 i8mm bf16 dgh bti ecv afp wfxt\n" \
  "CPU implementer\t: 0x41\n" \
  "CPU architecture: 8\n" \
  "CPU variant\t: 0x0\n" \
  "CPU part\t: 0xd87\n" \
  "CPU revision\t: 1\n" \
  "\n" \
  "processor\t: 4\n" \
  "BogoMIPS\t: 76.80\n" \
  "Features\t: fp asimd evtstrm aes pmull sha1 sha2 crc32 atomics fphp asimdhp cpuid asimdrdm jscvt fcma lrcpc dcpop sha3 sm3 sm4 asimddp sha512 sve asimdfhm dit uscat ilrcpc flagm sb paca pacg dcpodp sve2 sveaes svepmull svebitperm svesha3 svesm4 flagm2 frint svei8mm svebf16 i8mm bf16 dgh bti ecv afp wfxt\n" \
  "CPU implementer\t: 0x41\n" \
  "CPU architecture: 8\n" \
  "CPU variant\t: 0x0\n" \
  "CPU part\t: 0xd87\n" \
  "CPU revision\t: 1\n" \
  "\n" \
  "processor\t: 5\n" \
  "BogoMIPS\t: 76.80\n" \
  "Features\t: fp asimd evtstrm aes pmull sha1 sha2 crc32 atomics fphp asimdhp cpuid asimdrdm jscvt fcma lrcpc dcpop sha3 sm3 sm4 asimddp sha512 sve asimdfhm dit uscat ilrcpc flagm sb paca pacg dcpodp sve2 sveaes svepmull svebitperm svesha3 svesm4 flagm2 frint svei8mm svebf16 i8mm bf16 dgh bti ecv afp wfxt\n" \
  "CPU implementer\t: 0x41\n" \
  "CPU architecture: 8\n" \
  "CPU variant\t: 0x0\n" \
  "CPU part\t: 0xd87\n" \
  "CPU revision\t: 1\n" \
  "\n" \
  "processor\t: 6\n" \
  "BogoMIPS\t: 76.80\n" \
  "Features\t: fp asimd evtstrm aes pmull sha1 sha2 crc32 atomics fphp asimdhp cpuid asimdrdm jscvt fcma lrcpc dcpop sha3 sm3 sm4 asimddp sha512 sve asimdfhm dit uscat ilrcpc flagm sb paca pacg dcpodp sve2 sveaes svepmull svebitperm svesha3 svesm4 flagm2 frint svei8mm svebf16 i8mm bf16 dgh bti ecv afp wfxt\n" \
  "CPU implementer\t: 0x41\n" \
  "CPU architecture: 8\n" \
  "CPU variant\t: 0x0\n" \
  "CPU part\t: 0xd87\n" \
  "CPU revision\t: 1\n" \
  "\n" \
  "processor\t: 7\n" \
  "BogoMIPS\t: 76.80\n" \
  "Features\t: fp asimd evtstrm aes pmull sha1 sha2 crc32 atomics fphp asimdhp cpuid asimdrdm jscvt fcma lrcpc dcpop sha3 sm3 sm4 asimddp sha512 sve asimdfhm dit uscat ilrcpc flagm sb paca pacg dcpodp sve2 sveaes svepmull svebitperm svesha3 svesm4 flagm2 frint svei8mm svebf16 i8mm bf16 dgh bti ecv afp wfxt\n" \
  "CPU implementer\t: 0x41\n" \
  "CPU architecture: 8\n" \
  "CPU variant\t: 0x0\n" \
  "CPU part\t: 0xd87\n" \
  "CPU revision\t: 1\n"

// ============================================================
// GPU 伪造 — ARM Immortalis-G925
// ============================================================

#define FAKE_GPU_INFO           "Immortalis-G925"
#define FAKE_GPU_INFO_SIZE      15

#endif /* _XUANJIE_O1_H */
