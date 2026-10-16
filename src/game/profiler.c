/* stoptimer (RVA 0xb3f0): the end of a profiler timer (RDTSC cycles of the last tick), rewritten by hand.
 *
 * Phase 6 (docs/plano.md). The first version is the lifted C, unchanged. Each step makes
 * it readable and faster and keeps the harness at 0 differences: the float math keeps its
 * order and its (float) rounding points (x87, 53-bit precision, as the original).
 *
 * The function keeps the symbol lf_0b3f0: tools/lift/lift.py sees the definition here
 * and lifts it no more, but keeps it in the table, the live hooks and the standalone server.
 */
#include "lift_support.h"
#include "lift_os.h"

#define g_sysinfo (*(SysInfo *)SR_VA(0x956d2e0))
#define g_timer (*(Timer (*)[256])SR_VA(0x18043a0))
#define local_8 (*(undefined8 *)(lf_frame + 0x18))
void __cdecl lf_0b3f0(int param_1)

{
  __declspec(align(16)) uchar lf_frame[0x20];
  int *piVar1;
  uint uVar2;
  uint uVar3;
  int iVar4;
  undefined8 uVar5;
  ulonglong uVar6;
  double dVar7;
  uint uVar8;

  
  uVar5 = rdtsc();
                    
  (*(uint *)((uchar *)&local_8 + 0)) = (int)uVar5;
                    
  (*(uint *)((uchar *)&local_8 + 4)) = (int)((ulonglong)uVar5 >> 0x20);
  (*(uint *)((uchar *)&local_8 + 0)) = (uint)local_8;
  uVar3 = g_timer[param_1].unk_0040;
  uVar8 = (uint)(*(uint *)((uchar *)&local_8 + 0)) - g_timer[param_1].unk_0040;
  (*(uint *)((uchar *)&local_8 + 4)) = SUB84(local_8,4);
  iVar4 = g_timer[param_1].unk_0044;
  piVar1 = &g_timer[param_1].unk_0048;
  uVar2 = *piVar1;
  *piVar1 = *piVar1 + uVar8;
  g_timer[param_1].unk_004c =
       g_timer[param_1].unk_004c + (((*(uint *)((uchar *)&local_8 + 4)) - iVar4) - (uint)((uint)(*(uint *)((uchar *)&local_8 + 0)) < uVar3)) +
       (uint)CARRY4(uVar2,uVar8);
  (*(uint *)((uchar *)&uVar6 + 0)) = g_timer[param_1].unk_0048;
  (*(uint *)((uchar *)&uVar6 + 4)) = g_timer[param_1].unk_004c;
  local_8 = uVar6 & 0x7fffffffffffffff;
  dVar7 = (double)local_8;
  local_8 = (ulonglong)(g_timer[param_1].unk_004c & 0x80000000) << 0x20;
  (*(uint *)((uchar *)&local_8 + 0)) =
       (uint)(longlong)LF_CHOP((-(double)(longlong)local_8 + dVar7) * g_sysinfo.ms_per_tick);
  g_timer[param_1].unk_0050 = (uint)(*(uint *)((uchar *)&local_8 + 0));
  return;
}
#undef g_sysinfo
#undef g_timer
#undef local_8

