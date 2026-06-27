/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef _CPUINFO_KIRIN9020_H
#define _CPUINFO_KIRIN9020_H

#define MAX_TRACKED_FDS      64

#define FAKE_CPUINFO_SIZE    0xCA7

#define FAKE_CPUINFO_CONTENT \
  "Processor\t: AArch64 Processor rev 0 (aarch64)\n" \
  "processor\t: 0\n" \
  "BogoMIPS\t: 2000.00\n" \
  "Features\t: fp asimd evtstrm aes pmull sha1 sha2 crc32 atomics fphp asimdhp cpuid asimdrdm jscvt fcma lrcpc dcpop sha3 sm3 sm4 asimddp sha512 sve asimdfhm dit uscat ilrcpc flagm ssbs sb paca pacg dcpodp flagm2 frint svei8mm svebf16 i8mm bf16 dgh bti\n" \
  "CPU implementer\t: 0x48\n" \
  "CPU architecture: 8\n" \
  "CPU variant\t: 0x2\n" \
  "CPU part\t: 0xd24\n" \
  "CPU revision\t: 0\n" \
  "CPU physical\t: 0\n" \
  "\n" \
  "processor\t: 1\n" \
  "BogoMIPS\t: 2000.00\n" \
  "Features\t: fp asimd evtstrm aes pmull sha1 sha2 crc32 atomics fphp asimdhp cpuid asimdrdm jscvt fcma lrcpc dcpop sha3 sm3 sm4 asimddp sha512 sve asimdfhm dit uscat ilrcpc flagm ssbs sb paca pacg dcpodp flagm2 frint svei8mm svebf16 i8mm bf16 dgh bti\n" \
  "CPU implementer\t: 0x48\n" \
  "CPU architecture: 8\n" \
  "CPU variant\t: 0x2\n" \
  "CPU part\t: 0xd24\n" \
  "CPU revision\t: 0\n" \
  "CPU physical\t: 1\n" \
  "\n" \
  "processor\t: 2\n" \
  "BogoMIPS\t: 2000.00\n" \
  "Features\t: fp asimd evtstrm aes pmull sha1 sha2 crc32 atomics fphp asimdhp cpuid asimdrdm jscvt fcma lrcpc dcpop sha3 sm3 sm4 asimddp sha512 sve asimdfhm dit uscat ilrcpc flagm ssbs sb paca pacg dcpodp flagm2 frint svei8mm svebf16 i8mm bf16 dgh bti\n" \
  "CPU implementer\t: 0x48\n" \
  "CPU architecture: 8\n" \
  "CPU variant\t: 0x2\n" \
  "CPU part\t: 0xd24\n" \
  "CPU revision\t: 0\n" \
  "CPU physical\t: 2\n" \
  "\n" \
  "processor\t: 3\n" \
  "BogoMIPS\t: 2000.00\n" \
  "Features\t: fp asimd evtstrm aes pmull sha1 sha2 crc32 atomics fphp asimdhp cpuid asimdrdm jscvt fcma lrcpc dcpop sha3 sm3 sm4 asimddp sha512 sve asimdfhm dit uscat ilrcpc flagm ssbs sb paca pacg dcpodp flagm2 frint svei8mm svebf16 i8mm bf16 dgh bti\n" \
  "CPU implementer\t: 0x48\n" \
  "CPU architecture: 8\n" \
  "CPU variant\t: 0x2\n" \
  "CPU part\t: 0xd24\n" \
  "CPU revision\t: 0\n" \
  "CPU physical\t: 3\n" \
  "\n" \
  "processor\t: 4\n" \
  "BogoMIPS\t: 2000.00\n" \
  "Features\t: fp asimd evtstrm aes pmull sha1 sha2 crc32 atomics fphp asimdhp cpuid asimdrdm jscvt fcma lrcpc dcpop sha3 sm3 sm4 asimddp sha512 sve asimdfhm dit uscat ilrcpc flagm ssbs sb paca pacg dcpodp flagm2 frint svei8mm svebf16 i8mm bf16 dgh bti\n" \
  "CPU implementer\t: 0x48\n" \
  "CPU architecture: 8\n" \
  "CPU variant\t: 0x2\n" \
  "CPU part\t: 0xd47\n" \
  "CPU revision\t: 0\n" \
  "CPU physical\t: 4\n" \
  "\n" \
  "processor\t: 5\n" \
  "BogoMIPS\t: 2000.00\n" \
  "Features\t: fp asimd evtstrm aes pmull sha1 sha2 crc32 atomics fphp asimdhp cpuid asimdrdm jscvt fcma lrcpc dcpop sha3 sm3 sm4 asimddp sha512 sve asimdfhm dit uscat ilrcpc flagm ssbs sb paca pacg dcpodp flagm2 frint svei8mm svebf16 i8mm bf16 dgh bti\n" \
  "CPU implementer\t: 0x48\n" \
  "CPU architecture: 8\n" \
  "CPU variant\t: 0x2\n" \
  "CPU part\t: 0xd47\n" \
  "CPU revision\t: 0\n" \
  "CPU physical\t: 5\n" \
  "\n" \
  "processor\t: 6\n" \
  "BogoMIPS\t: 2000.00\n" \
  "Features\t: fp asimd evtstrm aes pmull sha1 sha2 crc32 atomics fphp asimdhp cpuid asimdrdm jscvt fcma lrcpc dcpop sha3 sm3 sm4 asimddp sha512 sve asimdfhm dit uscat ilrcpc flagm ssbs sb paca pacg dcpodp flagm2 frint svei8mm svebf16 i8mm bf16 dgh bti\n" \
  "CPU implementer\t: 0x48\n" \
  "CPU architecture: 8\n" \
  "CPU variant\t: 0x2\n" \
  "CPU part\t: 0xd47\n" \
  "CPU revision\t: 0\n" \
  "CPU physical\t: 6\n" \
  "\n" \
  "processor\t: 7\n" \
  "BogoMIPS\t: 2000.00\n" \
  "Features\t: fp asimd evtstrm aes pmull sha1 sha2 crc32 atomics fphp asimdhp cpuid asimdrdm jscvt fcma lrcpc dcpop sha3 sm3 sm4 asimddp sha512 sve asimdfhm dit uscat ilrcpc flagm ssbs sb paca pacg dcpodp flagm2 frint svei8mm svebf16 i8mm bf16 dgh bti\n" \
  "CPU implementer\t: 0x48\n" \
  "CPU architecture: 8\n" \
  "CPU variant\t: 0x2\n" \
  "CPU part\t: 0xd06\n" \
  "CPU revision\t: 0\n" \
  "CPU physical\t: 7\n" \
  "\n" \
  "Hardware\t: HUAWEI Kirin9030\n"

#endif /* _CPUINFO_KIRIN9020_H */