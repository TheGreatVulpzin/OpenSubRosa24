/* objectsimulation (RVA 0x56710): the vehicles and other objects: wheels, engine, rotors, train, collisions between objects, rewritten by hand.
 *
 * Phase 6 (docs/plano.md). The first version is the lifted C, unchanged. Each step makes
 * it readable and faster and keeps the harness at 0 differences: the float math keeps its
 * order and its (float) rounding points (x87, 53-bit precision, as the original).
 *
 * The function keeps the symbol lf_56710: tools/lift/lift.py sees the definition here
 * and lifts it no more, but keeps it in the table, the live hooks and the standalone server.
 */
#include "lift_support.h"
#include "lift_os.h"

#define g_objecttype (*(ObjectType (*)[128])SR_VA(0xb051bc0))
#define g_object (*(Object (*)[1024])SR_VA(0x5992600))
#define DAT_00460d18 (*(uint *)SR_VA(0x460d18))
#define DAT_1992d668 (*(undefined4 *)SR_VA(0x1992d668))
#define g_server (*(Server *)SR_VA(0x7a1ea60))
#define g_game (*(Game *)SR_VA(0x94dc540))
#define g_soundlist (*(SoundList *)SR_VA(0x956d700))
#define g_event (*(Event (*)[65536])SR_VA(0x97dd880))
#define g_particle (*(Particle (*)[4096])SR_VA(0xb7e7200))
#define g_city (*(City *)SR_VA(0xe699220))
#define g_numofevents (*(uint *)SR_VA(0xf837318))
#define g_intersect (*(Intersect *)SR_VA(0x107484a0))
#define g_timerlist (*(TimerList *)SR_VA(0x12e81c60))
#define g_track (*(Track *)SR_VA(0x25d7efc0))
#define g_yaxis (*(float (*)[3])SR_VA(0x460ce0))
#define g_numofbonds (*(int *)SR_VA(0x17579a28))
#define g_bond (*(Bond (*)[16384])SR_VA(0x17579a40))
#define g_timer (*(Timer (*)[256])SR_VA(0x18043a0))
#define stoptimer ((void (__cdecl *)(int))SR_FN(0x40b3f0))
#define rotateorientation ((void (__cdecl *)(float *, float *, float))SR_FN(0x40bdb0))
#define createevent_updateobject ((void (__cdecl *)(int, int, int, undefined4 *, undefined4 *))SR_FN(0x416ce0))
#define applydamageobject ((void (__cdecl *)(int, int))SR_FN(0x422e20))
#define objectsimulation_heli ((void (__cdecl *)(int))SR_FN(0x424f60))
#define lineintersectobject ((undefined4 (__cdecl *)(int, float *, float *, int))SR_FN(0x4257a0))
#define objecttrainsimulation ((void (__cdecl *)(int))SR_FN(0x426f90))
#define objectsimulation_car ((void (__cdecl *)(int))SR_FN(0x4272b0))
#define area_breakblock ((undefined4 (__cdecl *)(uint, uint, uint, uint, uint, int, undefined4 *, undefined4 *))SR_FN(0x42c020))
#define lineintersectlevel ((undefined4 (__cdecl *)(float *, float *, int))SR_FN(0x431630))
#define objectwheelcollision2 ((void (__cdecl *)(int, int))SR_FN(0x44e2c0))
#define objectwheelcollision ((void (__cdecl *)(int, int))SR_FN(0x44e500))
#define __security_check_cookie(x) ((void)0)
#define _ftol2(x) ((ulonglong)(longlong)(x))
#define _CIsqrt x87_sqrt
#define local_50 (*(float *)(lf_frame + 0x10))
#define local_4c (*(int *)(lf_frame + 0x14))
#define local_48 (*(int * *)(lf_frame + 0x18))
#define local_44 (*(int *)(lf_frame + 0x1c))
#define local_40 (*(float * *)(lf_frame + 0x20))
#define local_3c (*(float *)(lf_frame + 0x24))
#define local_38 (*(float *)(lf_frame + 0x28))
#define local_34 (*(float *)(lf_frame + 0x2c))
#define local_30 (*(float *)(lf_frame + 0x30))
#define local_2c (*(float *)(lf_frame + 0x34))
#define local_28 (*(float *)(lf_frame + 0x38))
#define local_24 (*(float *)(lf_frame + 0x3c))
#define local_20 (*(float *)(lf_frame + 0x40))
#define local_1c (*(float *)(lf_frame + 0x44))
#define local_18 (*(float *)(lf_frame + 0x48))
#define local_14 (*(float *)(lf_frame + 0x4c))
#define local_10 (*(float *)(lf_frame + 0x50))
#define local_c (*(float *)(lf_frame + 0x54))
#define local_8 (*(float *)(lf_frame + 0x58))
#define local_4 (*(uint *)(lf_frame + 0x5c))
void __cdecl lf_56710(void)

{
  __declspec(align(16)) uchar lf_frame[0x60];
  char cVar1;
  float fVar2;
  float fVar3;
  undefined8 uVar4;
  Event *pEVar5;
  Bond *pBVar6;
  int iVar7;
  float *pfVar8;
  float *pfVar9;
  uint uVar10;
  int iVar11;
  int iVar12;
  int iVar13;
  char *pcVar14;
  int iVar15;
  int *piVar16;
  float fVar17;
  uint uVar18;
  float *pfVar19;
  float *pfVar20;
  double dVar21;
  undefined4 uVar22;




















  float fVar7;
  
  iVar15 = g_timerlist.unk_008c;
  local_4 = DAT_00460d18 ^ (uint)&local_50;
  iVar7 = g_timerlist.unk_008c * 0x58;
  pcVar14 = ((char *)SR_VA(0x45f364));
  do {
    cVar1 = *pcVar14;
    pcVar14[iVar7 + 0x13a503c] = cVar1;
    pcVar14 = pcVar14 + 1;
  } while (cVar1 != '\0');
  g_timer[iVar15].unk_0048 = 0;
  g_timer[iVar15].unk_004c = 0;
  g_timer[iVar15].unk_0050 = 0;
  uVar4 = rdtsc();
  g_timer[iVar15].unk_0040 = (int)uVar4;
  g_timer[iVar15].unk_0044 = (int)((ulonglong)uVar4 >> 0x20);
  DAT_1992d668 = 0;
  local_44 = 0;
  local_2c = 0.0;
  local_30 = 0.0;
  pfVar20 = &g_object[0].vel.y;
  local_40 = &g_object[0].unk_0090;
  do {
    pfVar19 = local_40;
    if (LF_UINT(pfVar20[-0x19]) != 0x0U) {
      pfVar20[2] = ((vec3 *)(pfVar20 + -1))->x;
      iVar15 = 3;
      pfVar20[3] = *pfVar20;
      pfVar20[4] = pfVar20[1];
      pfVar20[-0x10] = 0.0;
      pfVar20[-0xf] = 0.0;
      pfVar20[-0xe] = 0.0;
      ((vec3 *)(pfVar20 + -1))->x = 0.0;
      *pfVar20 = 0.0;
      pfVar20[1] = 0.0;
      local_48 = (int *)0x0;
      pfVar8 = pfVar20 + 0xb;
      do {
        iVar15 = iVar15 + -1;
        pfVar8[-3] = 65536.0;
        *pfVar8 = -65536.0;
        pfVar8 = pfVar8 + 1;
      } while (iVar15 != 0);
      local_4c = 0;
      if (0 < ((int)LF_UINT(pfVar20[0x11]))) {
        pfVar8 = pfVar20 + 0x12;
        local_4c = 0;
        do {
          LF_COPY(fVar2, *pfVar8);
          local_28 = (float)((&g_particle[0].unk_0044)[((int)LF_UINT(fVar2)) * 0x3c] -
                     (&g_particle[0].vel.x)[((int)LF_UINT(fVar2)) * 0x3c]);
          local_24 = (float)((&g_particle[0].unk_0048)[((int)LF_UINT(fVar2)) * 0x3c] -
                     (&g_particle[0].vel.y)[((int)LF_UINT(fVar2)) * 0x3c]);
          local_20 = (float)((&g_particle[0].unk_004c)[((int)LF_UINT(fVar2)) * 0x3c] -
                     (&g_particle[0].vel.z)[((int)LF_UINT(fVar2)) * 0x3c]);
          local_50 = (float)(local_20 * local_20 + (local_28 * local_28 + local_24 * local_24));
          dVar21 = _CIsqrt((double)local_50);
          local_48 = ((int *)LF_BITS_U(((float)((float)dVar21 + LF_BITS_F(local_48)))));
          (&g_particle[0].unk_0044)[((int)LF_UINT(fVar2)) * 0x3c] =
               (&g_particle[0].vel.x)[((int)LF_UINT(fVar2)) * 0x3c];
          (&g_particle[0].unk_0048)[((int)LF_UINT(fVar2)) * 0x3c] =
               (&g_particle[0].vel.y)[((int)LF_UINT(fVar2)) * 0x3c];
          (&g_particle[0].unk_004c)[((int)LF_UINT(fVar2)) * 0x3c] =
               (&g_particle[0].vel.z)[((int)LF_UINT(fVar2)) * 0x3c];
          pfVar20[-0x10] = (float)(pfVar20[-0x10] + (&g_particle[0].pos.x)[((int)LF_UINT(fVar2)) * 0x3c]);
          pfVar20[-0xf] = (float)((&g_particle[0].pos.y)[((int)LF_UINT(fVar2)) * 0x3c] + pfVar20[-0xf]);
          pfVar20[-0xe] = (float)((&g_particle[0].pos.z)[((int)LF_UINT(fVar2)) * 0x3c] + pfVar20[-0xe]);
          LF_COPY(fVar2, *pfVar8);
          ((vec3 *)(pfVar20 + -1))->x =
               (float)((&g_particle[0].vel.x)[((int)LF_UINT(fVar2)) * 0x3c] + ((vec3 *)(pfVar20 + -1))->x);
          *pfVar20 = (float)((&g_particle[0].vel.y)[((int)LF_UINT(fVar2)) * 0x3c] + *pfVar20);
          pfVar20[1] = (float)((&g_particle[0].vel.z)[((int)LF_UINT(fVar2)) * 0x3c] + pfVar20[1]);
          if ((&g_particle[0].pos.x)[((int)LF_UINT(*pfVar8)) * 0x3c] < pfVar19[-3]) {
            pfVar19[-3] = (&g_particle[0].pos.x)[((int)LF_UINT(*pfVar8)) * 0x3c];
          }
          if (*pfVar19 < (&g_particle[0].pos.x)[((int)LF_UINT(*pfVar8)) * 0x3c]) {
            *pfVar19 = (&g_particle[0].pos.x)[((int)LF_UINT(*pfVar8)) * 0x3c];
          }
          if ((&g_particle[0].pos.y)[((int)LF_UINT(*pfVar8)) * 0x3c] < pfVar19[-2]) {
            pfVar19[-2] = (&g_particle[0].pos.y)[((int)LF_UINT(*pfVar8)) * 0x3c];
          }
          if (pfVar19[1] < (&g_particle[0].pos.y)[((int)LF_UINT(*pfVar8)) * 0x3c]) {
            pfVar19[1] = (&g_particle[0].pos.y)[((int)LF_UINT(*pfVar8)) * 0x3c];
          }
          if ((&g_particle[0].pos.z)[((int)LF_UINT(*pfVar8)) * 0x3c] < pfVar19[-1]) {
            pfVar19[-1] = (&g_particle[0].pos.z)[((int)LF_UINT(*pfVar8)) * 0x3c];
          }
          if (pfVar19[2] < (&g_particle[0].pos.z)[((int)LF_UINT(*pfVar8)) * 0x3c]) {
            pfVar19[2] = (&g_particle[0].pos.z)[((int)LF_UINT(*pfVar8)) * 0x3c];
          }
          local_4c = local_4c + 1;
          pfVar8 = pfVar8 + 1;
        } while (local_4c < ((int)LF_UINT(pfVar20[0x11])));
      }
      iVar15 = 0;
      if (0 < ((int)LF_UINT(pfVar20[0x17a1]))) {
        pfVar8 = pfVar20 + 0x17aa;
        pfVar9 = pfVar20 + 0x17b1;
        do {
          if (pfVar9[-1] - *pfVar8 < pfVar19[-3]) {
            pfVar19[-3] = pfVar9[-1] - *pfVar8;
          }
          if (*pfVar19 < *pfVar8 + pfVar9[-1]) {
            *pfVar19 = *pfVar8 + pfVar9[-1];
          }
          if (*pfVar9 - *pfVar8 < pfVar19[-2]) {
            pfVar19[-2] = *pfVar9 - *pfVar8;
          }
          if (pfVar19[1] < *pfVar9 + *pfVar8) {
            pfVar19[1] = *pfVar9 + *pfVar8;
          }
          if (pfVar9[1] - *pfVar8 < pfVar19[-1]) {
            pfVar19[-1] = pfVar9[1] - *pfVar8;
          }
          if (pfVar19[2] < *pfVar8 + pfVar9[1]) {
            pfVar19[2] = *pfVar8 + pfVar9[1];
          }
          iVar15 = iVar15 + 1;
          pfVar9 = pfVar9 + 0x26;
          pfVar8 = pfVar8 + 0x26;
        } while (iVar15 < ((int)LF_UINT(pfVar20[0x17a1])));
      }
      fVar2 = (float)(1.0 / (float)((int)LF_UINT(pfVar20[0x11])));
      pfVar20[-0x10] = fVar2 * pfVar20[-0x10];
      pfVar20[-0xf] = (float)(pfVar20[-0xf] * fVar2);
      pfVar20[-0xe] = (float)(fVar2 * pfVar20[-0xe]);
      fVar2 = (float)(1.0 / (float)((int)LF_UINT(pfVar20[0x11])));
      ((vec3 *)(pfVar20 + -1))->x = fVar2 * ((vec3 *)(pfVar20 + -1))->x;
      *pfVar20 = (float)(*pfVar20 * fVar2);
      pfVar20[1] = (float)(fVar2 * pfVar20[1]);
      LF_COPY(fVar2, pfVar20[0x52]);
      local_48 = ((int *)LF_BITS_U(((float)(LF_BITS_F(local_48) / (float)((int)LF_UINT(pfVar20[0x11]))))));
      local_3c = 0.0;
      local_38 = 0.0;
      local_34 = 0.0;
      if (0 < ((int)LF_UINT(fVar2))) {
        pfVar8 = pfVar20 + 0x12;
        LF_COPY(fVar17, fVar2);
        do {
          LF_COPY(fVar3, *pfVar8);
          pfVar8 = pfVar8 + 1;
          LF_SET_BITS(fVar17, (((int)LF_UINT(fVar17)) + -1));
          local_3c = (float)((&g_particle[0].pos.x)[((int)LF_UINT(fVar3)) * 0x3c] + local_3c);
          local_38 = (float)((&g_particle[0].pos.y)[((int)LF_UINT(fVar3)) * 0x3c] + local_38);
          local_34 = (float)((&g_particle[0].pos.z)[((int)LF_UINT(fVar3)) * 0x3c] + local_34);
        } while (LF_UINT(fVar17) != 0x0U);
      }
      iVar15 = 0;
      fVar2 = (float)(1.0 / (float)((int)LF_UINT(fVar2)));
      local_3c = (float)(fVar2 * local_3c);
      local_38 = (float)(local_38 * fVar2);
      local_34 = (float)(fVar2 * local_34);
      pfVar20[-10] = 0.0;
      pfVar20[-9] = 0.0;
      pfVar20[-8] = 0.0;
      pfVar20[-4] = 0.0;
      pfVar20[-3] = 0.0;
      pfVar20[-2] = 0.0;
      if (0 < ((int)LF_UINT(pfVar20[0x52]))) {
        pfVar8 = pfVar20 + 0x53;
        do {
          LF_COPY(fVar2, pfVar8[-0x41]);
          iVar15 = iVar15 + 1;
          local_10 = (float)((&g_particle[0].pos.x)[((int)LF_UINT(fVar2)) * 0x3c] - local_3c);
          local_c = (float)((&g_particle[0].pos.y)[((int)LF_UINT(fVar2)) * 0x3c] - local_38);
          local_8 = (float)((&g_particle[0].pos.z)[((int)LF_UINT(fVar2)) * 0x3c] - local_34);
          LF_COPY(fVar2, *pfVar8);
          pfVar20[-10] = (float)(pfVar20[-10] + fVar2 * local_10);
          pfVar20[-9] = (float)(pfVar20[-9] + local_c * fVar2);
          pfVar20[-8] = (float)(pfVar20[-8] + local_8 * fVar2);
          LF_COPY(fVar2, pfVar8[0x40]);
          pfVar20[-4] = (float)(pfVar20[-4] + fVar2 * local_10);
          pfVar20[-3] = (float)(local_c * fVar2 + pfVar20[-3]);
          pfVar20[-2] = (float)(local_8 * fVar2 + pfVar20[-2]);
          pfVar8 = pfVar8 + 1;
        } while (iVar15 < ((int)LF_UINT(pfVar20[0x52])));
      }
      local_50 = (float)(pfVar20[-8] * pfVar20[-8] + (pfVar20[-10] * pfVar20[-10] + pfVar20[-9] * pfVar20[-9]))
      ;
      dVar21 = _CIsqrt((double)local_50);
      local_4c = ((int)LF_BITS_U((float)dVar21));
      fVar2 = 0.0;
      if (LF_BITS_F(local_4c) == 0.0) {
        pfVar20[-10] = 0.0;
        pfVar20[-9] = 0.0;
      }
      else {
        local_4c = ((int)LF_BITS_U(((float)(1.0 / LF_BITS_F(local_4c)))));
        pfVar20[-10] = LF_BITS_F(local_4c) * pfVar20[-10];
        pfVar20[-9] = (float)(pfVar20[-9] * LF_BITS_F(local_4c));
        fVar2 = LF_BITS_F(local_4c) * pfVar20[-8];
      }
      pfVar20[-8] = fVar2;
      local_50 = (float)(pfVar20[-2] * pfVar20[-2] + (pfVar20[-4] * pfVar20[-4] + pfVar20[-3] * pfVar20[-3]));
      dVar21 = _CIsqrt((double)local_50);
      local_4c = ((int)LF_BITS_U((float)dVar21));
      fVar2 = 0.0;
      if (LF_BITS_F(local_4c) == 0.0) {
        pfVar20[-4] = 0.0;
        pfVar20[-3] = 0.0;
      }
      else {
        local_4c = ((int)LF_BITS_U(((float)(1.0 / LF_BITS_F(local_4c)))));
        pfVar20[-4] = LF_BITS_F(local_4c) * pfVar20[-4];
        pfVar20[-3] = (float)(LF_BITS_F(local_4c) * pfVar20[-3]);
        fVar2 = LF_BITS_F(local_4c) * pfVar20[-2];
      }
      pfVar20[-2] = fVar2;
      pfVar20[-7] = (float)(pfVar20[-8] * pfVar20[-3] - pfVar20[-9] * pfVar20[-2]);
      pfVar20[-6] = (float)(pfVar20[-2] * pfVar20[-10] - pfVar20[-4] * pfVar20[-8]);
      pfVar20[-5] = (float)(pfVar20[-9] * pfVar20[-4] - pfVar20[-10] * pfVar20[-3]);
      local_50 = (float)(pfVar20[-5] * pfVar20[-5] + (pfVar20[-7] * pfVar20[-7] + pfVar20[-6] * pfVar20[-6]));
      dVar21 = _CIsqrt((double)local_50);
      local_4c = ((int)LF_BITS_U((float)dVar21));
      fVar2 = 0.0;
      if (LF_BITS_F(local_4c) == 0.0) {
        pfVar20[-7] = 0.0;
        pfVar20[-6] = 0.0;
      }
      else {
        local_4c = ((int)LF_BITS_U(((float)(1.0 / LF_BITS_F(local_4c)))));
        pfVar20[-7] = (float)(LF_BITS_F(local_4c) * pfVar20[-7]);
        pfVar20[-6] = (float)(LF_BITS_F(local_4c) * pfVar20[-6]);
        fVar2 = LF_BITS_F(local_4c) * pfVar20[-5];
      }
      pfVar20[-5] = fVar2;
      pfVar20[-10] = (float)(pfVar20[-2] * pfVar20[-6] - pfVar20[-5] * pfVar20[-3]);
      pfVar20[-9] = (float)(pfVar20[-5] * pfVar20[-4] - pfVar20[-2] * pfVar20[-7]);
      pfVar20[-8] = (float)(pfVar20[-3] * pfVar20[-7] - pfVar20[-4] * pfVar20[-6]);
      local_50 = (float)(pfVar20[-8] * pfVar20[-8] + (pfVar20[-10] * pfVar20[-10] + pfVar20[-9] * pfVar20[-9]))
      ;
      dVar21 = _CIsqrt((double)local_50);
      local_4c = ((int)LF_BITS_U((float)dVar21));
      if (LF_BITS_F(local_4c) == 0.0) {
        pfVar20[-10] = 0.0;
        pfVar20[-9] = 0.0;
        pfVar20[-8] = 0.0;
      }
      else {
        local_4c = ((int)LF_BITS_U(((float)(1.0 / LF_BITS_F(local_4c)))));
        pfVar20[-10] = LF_BITS_F(local_4c) * pfVar20[-10];
        pfVar20[-9] = (float)(pfVar20[-9] * LF_BITS_F(local_4c));
        pfVar20[-8] = (float)(LF_BITS_F(local_4c) * pfVar20[-8]);
      }
      if (pfVar20[-0xf] < -32.0) {
        pfVar20[-0x12] = 0.0;
      }
      LF_COPY(fVar2, pfVar20[-0x18]);
      fVar17 = 0.0;
      if (0 < ((int)LF_UINT(pfVar20[0x11]))) {
        pfVar8 = pfVar20 + 0x12;
        pfVar9 = pfVar20 + 0x12d8;
        do {
          LF_COPY(fVar3, *pfVar8);
          pfVar8 = pfVar8 + 1;
          pfVar9[-2] = (&g_particle[0].pos.x)[((int)LF_UINT(fVar3)) * 0x3c];
          pfVar9[-1] = (&g_particle[0].pos.y)[((int)LF_UINT(fVar3)) * 0x3c];
          *pfVar9 = (&g_particle[0].pos.z)[((int)LF_UINT(fVar3)) * 0x3c];
          pfVar9[1] = fVar17;
          LF_SET_BITS(fVar17, (((int)LF_UINT(fVar17)) + 1));
          pfVar9 = pfVar9 + 4;
        } while (((int)LF_UINT(fVar17)) < ((int)LF_UINT(pfVar20[0x11])));
      }
      LF_COPY(fVar17, pfVar20[0x11]);
      if (((int)LF_UINT(fVar17)) < g_objecttype[((int)LF_UINT(fVar2))].num_quads) {
        pfVar8 = (float *)(((int)LF_UINT(fVar17)) * 0x20 + ((int)&g_objecttype[0].lf_raw[0x5360]) + ((int)LF_UINT(fVar2)) * 0x828c);
        iVar15 = g_objecttype[((int)LF_UINT(fVar2))].num_quads - ((int)LF_UINT(fVar17));
        pfVar9 = (float *)(&g_object[0].lf_raw[0x4bc4] + (((int)LF_UINT(local_2c)) + ((int)LF_UINT(fVar17))) * 0x10);
        do {
          LF_COPY(fVar17, pfVar8[-5]);
          LF_COPY(fVar2, pfVar8[-1]);
          iVar7 = *(int *)((int)(g_object[0].rot + 9) + (((int)LF_UINT(local_30)) + ((int)LF_UINT(fVar17)) + 1) * 4);
          pfVar9[-2] = (float)(fVar2 * (&g_particle[0].pos.x)[iVar7 * 0x3c]);
          pfVar9[-1] = (float)((&g_particle[0].pos.y)[iVar7 * 0x3c] * fVar2);
          LF_COPY(fVar3, pfVar8[-4]);
          *pfVar9 = (float)(fVar2 * (&g_particle[0].pos.z)[iVar7 * 0x3c]);
          iVar7 = *(int *)((int)(g_object[0].rot + 9) + (((int)LF_UINT(fVar3)) + ((int)LF_UINT(local_30)) + 1) * 4);
          LF_COPY(fVar2, *pfVar8);
          pfVar9[-2] = (float)(pfVar9[-2] + fVar2 * (&g_particle[0].pos.x)[iVar7 * 0x3c]);
          pfVar9[-1] = (float)((&g_particle[0].pos.y)[iVar7 * 0x3c] * fVar2 + pfVar9[-1]);
          LF_COPY(fVar3, pfVar8[-3]);
          *pfVar9 = (float)(fVar2 * (&g_particle[0].pos.z)[iVar7 * 0x3c] + *pfVar9);
          iVar7 = *(int *)((int)(g_object[0].rot + 9) + (((int)LF_UINT(fVar3)) + ((int)LF_UINT(local_30)) + 1) * 4);
          LF_COPY(fVar2, pfVar8[1]);
          pfVar9[-2] = (float)(fVar2 * (&g_particle[0].pos.x)[iVar7 * 0x3c] + pfVar9[-2]);
          pfVar9[-1] = (float)((&g_particle[0].pos.y)[iVar7 * 0x3c] * fVar2 + pfVar9[-1]);
          LF_COPY(fVar3, pfVar8[-2]);
          *pfVar9 = (float)(fVar2 * (&g_particle[0].pos.z)[iVar7 * 0x3c] + *pfVar9);
          iVar7 = *(int *)((int)(g_object[0].rot + 9) + (((int)LF_UINT(fVar3)) + ((int)LF_UINT(local_30)) + 1) * 4);
          local_50 = pfVar8[2];
          pfVar9[-2] = (float)(pfVar9[-2] + local_50 * (&g_particle[0].pos.x)[iVar7 * 0x3c]);
          pfVar9[-1] = (float)((&g_particle[0].pos.y)[iVar7 * 0x3c] * local_50 + pfVar9[-1]);
          *pfVar9 = (float)(local_50 * (&g_particle[0].pos.z)[iVar7 * 0x3c] + *pfVar9);
          pfVar9[1] = fVar17;
          pfVar8 = pfVar8 + 8;
          iVar15 = iVar15 + -1;
          pfVar9 = pfVar9 + 4;
          pfVar19 = local_40;
        } while (iVar15 != 0);
      }
      if (LF_UINT(pfVar20[-0x18]) != 0xdU) {
        if (0.0833333358168602 < LF_BITS_F(local_48)) {
          applydamageobject(local_44,(int)(longlong)(LF_BITS_F(local_48) * 60.0 + 10.0));
        }
        uVar10 = g_numofevents;
        iVar15 = g_game.ticks;
        if (LF_BITS_F(local_48) <= 0.03333333507180214) {
          if ((0.5 < pfVar20[0x206a]) && (g_server.unk_0000 != 0)) {
            LF_COPY(fVar2, pfVar20[-0x10]);
            g_event[g_numofevents].type = 9;
            g_event[g_numofevents].tick = iVar15;
            g_event[g_numofevents].param = g_soundlist.unk_007c;
            g_event[g_numofevents].pos.x = fVar2;
            g_event[g_numofevents].pos.y = pfVar20[-0xf];
            pEVar5 = g_event + g_numofevents;
            (*(ushort *)((uchar *)&g_numofevents + 0)) = (short)g_numofevents + 1;
            (pEVar5->pos).z = pfVar20[-0xe];
            g_event[uVar10].sound_a = (float)(pfVar20[0x206a] * 0.5);
            g_event[uVar10].sound_b = 1.0;
            g_numofevents = (uint)(ushort)g_numofevents;
          }
          pfVar20[0x206a] = 0.0;
        }
        else {
          local_50 = (float)(LF_BITS_F(local_48) * 60.0 * 0.125 + pfVar20[0x206a]);
          pfVar20[0x206a] = local_50;
          uVar10 = g_numofevents;
          iVar15 = g_game.ticks;
          if (2.0 < local_50) {
            if (g_server.unk_0000 != 0) {
              LF_COPY(fVar2, pfVar20[-0x10]);
              g_event[g_numofevents].type = 9;
              g_event[g_numofevents].tick = iVar15;
              g_event[g_numofevents].param = g_soundlist.unk_007c;
              g_event[g_numofevents].pos.x = fVar2;
              g_event[g_numofevents].pos.y = pfVar20[-0xf];
              pEVar5 = g_event + g_numofevents;
              (*(ushort *)((uchar *)&g_numofevents + 0)) = (short)g_numofevents + 1;
              (pEVar5->pos).z = pfVar20[-0xe];
              g_event[uVar10].sound_a = (float)(local_50 * 0.5);
              g_event[uVar10].sound_b = 1.0;
              g_numofevents = (uint)(ushort)g_numofevents;
            }
            pfVar20[0x206a] = (float)(local_50 - 2.0);
          }
        }
      }
      if ((LF_UINT(pfVar20[-0x17]) != 0x2U) && (0 < ((int)LF_UINT(pfVar20[0x11])))) {
        local_40 = ((float *)LF_UINT(pfVar20[0x11]));
        pfVar8 = pfVar20 + 0x12;
        do {
          LF_COPY(fVar2, *pfVar8);
          pfVar9 = &g_particle[0].vel.x + ((int)LF_UINT(fVar2)) * 0x3c;
          local_50 = (float)((&g_particle[0].vel.z)[((int)LF_UINT(fVar2)) * 0x3c] *
                     (&g_particle[0].vel.z)[((int)LF_UINT(fVar2)) * 0x3c] +
                     ((&g_particle[0].vel.x)[((int)LF_UINT(fVar2)) * 0x3c] *
                     (&g_particle[0].vel.x)[((int)LF_UINT(fVar2)) * 0x3c] +
                     (&g_particle[0].vel.y)[((int)LF_UINT(fVar2)) * 0x3c] *
                     (&g_particle[0].vel.y)[((int)LF_UINT(fVar2)) * 0x3c]));
          dVar21 = _CIsqrt((double)local_50);
          local_8 = (float)dVar21;
          local_48 = ((int *)LF_BITS_U(((float)(local_8 * local_8))));
          if (local_8 == 0.0) {
            local_10 = 0.0;
            local_c = 0.0;
            local_8 = 0.0;
          }
          else {
            local_8 = (float)(1.0 / local_8);
            local_10 = (float)(local_8 * *pfVar9);
            local_c = (float)(local_8 * (&g_particle[0].vel.y)[((int)LF_UINT(fVar2)) * 0x3c]);
            local_8 = (float)(local_8 * (&g_particle[0].vel.z)[((int)LF_UINT(fVar2)) * 0x3c]);
          }
          fVar17 = ((LF_BITS_F(local_48) * 4.0 * 0.5 * 0.75) / -1000.0) *
                   ((float)(local_8 * pfVar20[-8] + (local_10 * pfVar20[-10] + local_c * pfVar20[-9])));
          *pfVar9 = (float)(*pfVar9 + fVar17 * pfVar20[-10]);
          (&g_particle[0].vel.y)[((int)LF_UINT(fVar2)) * 0x3c] =
               (float)(pfVar20[-9] * fVar17 + (&g_particle[0].vel.y)[((int)LF_UINT(fVar2)) * 0x3c]);
          (&g_particle[0].vel.z)[((int)LF_UINT(fVar2)) * 0x3c] =
               (float)(fVar17 * pfVar20[-8] + (&g_particle[0].vel.z)[((int)LF_UINT(fVar2)) * 0x3c]);
          fVar17 = (float)((((LF_BITS_F(local_48) + LF_BITS_F(local_48)) * 4.0 * 0.5 * 0.75) / -1000.0) *
                   ((float)(local_8 * pfVar20[-5] + (local_c * pfVar20[-6] + local_10 * pfVar20[-7]))));
          *pfVar9 = (float)(fVar17 * pfVar20[-7] + *pfVar9);
          (&g_particle[0].vel.y)[((int)LF_UINT(fVar2)) * 0x3c] =
               (float)(fVar17 * pfVar20[-6] + (&g_particle[0].vel.y)[((int)LF_UINT(fVar2)) * 0x3c]);
          (&g_particle[0].vel.z)[((int)LF_UINT(fVar2)) * 0x3c] =
               (float)(fVar17 * pfVar20[-5] + (&g_particle[0].vel.z)[((int)LF_UINT(fVar2)) * 0x3c]);
          local_50 = (float)(local_8 * pfVar20[-2] + (pfVar20[-4] * local_10 + pfVar20[-3] * local_c));
          pfVar8 = pfVar8 + 1;
          local_40 = (float *)((int)local_40 + -1);
          local_4c = ((int)LF_BITS_U(((float)((((LF_BITS_F(local_48) + LF_BITS_F(local_48)) * 0.25 * 0.75) / -1000.0) *
                          local_50))));
          *pfVar9 = (float)(LF_BITS_F(local_4c) * pfVar20[-4] + *pfVar9);
          (&g_particle[0].vel.y)[((int)LF_UINT(fVar2)) * 0x3c] =
               (float)(LF_BITS_F(local_4c) * pfVar20[-3] + (&g_particle[0].vel.y)[((int)LF_UINT(fVar2)) * 0x3c]);
          (&g_particle[0].vel.z)[((int)LF_UINT(fVar2)) * 0x3c] =
               (float)(LF_BITS_F(local_4c) * pfVar20[-2] + (&g_particle[0].vel.z)[((int)LF_UINT(fVar2)) * 0x3c]);
        } while (local_40 != (float *)0x0);
      }
    }
    local_44 = local_44 + 1;
    LF_SET_BITS(local_30, (((int)LF_UINT(local_30)) + 0x208c));
    LF_SET_BITS(local_2c, (((int)LF_UINT(local_2c)) + 0x823));
    local_40 = pfVar19 + 0x208c;
    pfVar20 = pfVar20 + 0x208c;
  } while ((int)local_40 < ((int)SR_VA(0x7a1e690)));
  iVar15 = 0;
  local_44 = 0;
  local_40 = (float *)0x0;
  do {
    if (*(int *)((int)&((Object *)(g_object[0].rot + -5))->active + iVar15) != 0) {
      if (*(int *)((int)g_object[0].rot + iVar15 + -0x34) == 1) {
        objectsimulation_car(local_44);
      }
      if (*(int *)((int)g_object[0].rot + iVar15 + -0x34) == 2) {
        objectsimulation_heli(local_44);
      }
      if (*(int *)((int)g_object[0].rot + iVar15 + -0x38) == 0xc) {
        pfVar20 = (float *)((int)&g_object[0].rot[-2].x + iVar15);
        local_10 = (float)(g_city.unk_01b4 - *pfVar20);
        pfVar19 = (float *)((int)&g_object[0].rot[3].x + iVar15);
        local_8 = (float)(g_city.unk_01bc - *(float *)((int)g_object[0].rot + iVar15 + -0x10));
        local_c = 0.0;
        if (0.0 < (float)(local_8 * *(float *)((int)&g_object[0].rot[3].z + iVar15) +
                  (local_10 * *pfVar19 + *(float *)((int)&g_object[0].rot[3].y + iVar15) * 0.0))) {
          local_50 = (float)(local_10 * local_10 + 0.0 + local_8 * local_8);
          dVar21 = _CIsqrt((double)local_50);
          if ((float)dVar21 < 160.0) {
            DAT_1992d668 = 1;
          }
        }
        local_4c = 0x3ecccccd;
        *(int *)((int)&g_object[0].unk_5ee0 + iVar15) =
             (int)(longlong)
                  (ABS((float)(*(float *)((int)&g_object[0].rot[2].z + iVar15) *
                       *(float *)((int)&g_object[0].rot[3].z + iVar15) +
                       (*(float *)((int)&g_object[0].rot[2].x + iVar15) * *pfVar19 +
                       *(float *)((int)&g_object[0].rot[3].y + iVar15) *
                       *(float *)((int)&g_object[0].rot[2].y + iVar15)))) * 120.0 * 60.0);
        local_50 = (float)(*(float *)((int)&g_object[0].rot[2].z + iVar15) *
                   *(float *)((int)&g_object[0].rot[3].z + iVar15) +
                   (*(float *)((int)&g_object[0].rot[2].x + iVar15) * *pfVar19 +
                   *(float *)((int)&g_object[0].rot[3].y + iVar15) *
                   *(float *)((int)&g_object[0].rot[2].y + iVar15)));
        local_48 = ((int *)LF_BITS_U(-local_50));
        if (0.01666666753590107 <= LF_BITS_F(local_48)) {
          if (*(int *)(g_objecttype[0xc].unk_8288 * 0x40 + ((int)&g_object[0].lf_raw[0x7f4]) + iVar15) < 8) {
            *(undefined4 *)(g_objecttype[0xc].unk_8288 * 0x40 + ((int)&g_object[0].lf_raw[0x7f4]) + iVar15) = 8;
            iVar7 = g_objecttype[*(int *)((int)g_object[0].rot + iVar15 + -0x38)].unk_8288;
            uVar22 = 5;
            goto LAB_00457632;
          }
        }
        else if (0 < *(int *)(g_objecttype[0xc].unk_8288 * 0x40 + ((int)&g_object[0].lf_raw[0x7f4]) + iVar15)) {
          *(undefined4 *)(g_objecttype[0xc].unk_8288 * 0x40 + ((int)&g_object[0].lf_raw[0x7f4]) + iVar15) = 0;
          iVar7 = g_objecttype[*(int *)((int)g_object[0].rot + iVar15 + -0x38)].unk_8288;
          uVar22 = 4;
LAB_00457632:
          createevent_updateobject(local_44,uVar22,iVar7,pfVar20,pfVar19);
        }
        iVar7 = *(int *)((int)&g_object[0].unk_8194 + iVar15);
        local_28 = (float)((&g_track.unk_0004)[iVar7 * 0x15e] - *pfVar20);
        local_24 = (float)((&g_track.unk_0008)[iVar7 * 0x15e] -
                   *(float *)((int)g_object[0].rot + iVar15 + -0x14));
        local_20 = (float)((&g_track.unk_000c)[iVar7 * 0x15e] -
                   *(float *)((int)g_object[0].rot + iVar15 + -0x10));
        local_50 = (float)(local_20 * local_20 + (local_28 * local_28 + local_24 * local_24));
        dVar21 = _CIsqrt((double)local_50);
        if (((float)dVar21 < 128.0) &&
           (uVar10 = *(int *)((int)&g_object[0].unk_819c + iVar15) - iVar7,
           uVar18 = (int)uVar10 >> 0x1f, (int)((uVar10 ^ uVar18) - uVar18) < 8)) {
          local_4c = 0;
          if (LF_BITS_F(local_48) < 0.01666666753590107) {
            piVar16 = (int *)((int)&g_object[0].unk_8198 + iVar15);
            *piVar16 = *piVar16 + 1;
            if (*(int *)((int)&g_object[0].unk_8198 + iVar15) < 0x4b0) goto LAB_0045770a;
            *(uint *)((int)&g_object[0].unk_8194 + iVar15) = (-(uint)(iVar7 != 0x4a) & 0x2e) + 0x1c;
          }
          *(undefined4 *)((int)&g_object[0].unk_8198 + iVar15) = 0;
        }
LAB_0045770a:
        local_50 = (float)(LF_BITS_F(local_4c) - LF_BITS_F(local_48));
        local_4c = ((int)LF_BITS_U((local_50 * 8.0)));
        if (-1.0 <= LF_BITS_F(local_4c)) {
          if (1.0 < LF_BITS_F(local_4c)) {
            local_4c = 0x3f800000;
          }
        }
        else {
          local_4c = -0x40800000;
        }
        if (0 < *(int *)((int)(g_object[0].rot + 9) + iVar15)) {
          iVar7 = *(int *)((int)(g_object[0].rot + 9) + iVar15);
          piVar16 = (int *)((int)(g_object[0].rot + 9) + iVar15 + 4);
          local_50 = (float)((-LF_BITS_F(local_4c) * 8.0) / 3600.0);
          fVar2 = *(float *)((int)&g_object[0].rot[2].x + iVar15);
          fVar17 = *(float *)((int)&g_object[0].rot[2].y + iVar15);
          fVar3 = *(float *)((int)&g_object[0].rot[2].z + iVar15);
          do {
            iVar13 = *piVar16;
            (&g_particle[0].vel.x)[iVar13 * 0x3c] =
                 (float)((&g_particle[0].vel.x)[iVar13 * 0x3c] + local_50 * fVar2);
            piVar16 = piVar16 + 1;
            iVar7 = iVar7 + -1;
            (&g_particle[0].vel.y)[iVar13 * 0x3c] =
                 (float)((&g_particle[0].vel.y)[iVar13 * 0x3c] + local_50 * fVar17);
            (&g_particle[0].vel.z)[iVar13 * 0x3c] =
                 (float)((&g_particle[0].vel.z)[iVar13 * 0x3c] + fVar3 * local_50);
          } while (iVar7 != 0);
        }
      }
      iVar7 = 0;
      if (0 < *(int *)((int)&g_object[0].unk_71ec + iVar15)) {
        pfVar20 = (float *)(&g_object[0].lf_raw[0x71f4] + iVar15);
        do {
          pfVar19 = pfVar20 + 8;
          pfVar20[-1] = (float)(pfVar20[2] + pfVar20[-1]);
          *pfVar20 = (float)(*pfVar20 + pfVar20[3]);
          pfVar20[1] = (float)(pfVar20[4] + pfVar20[1]);
          pfVar20[3] = (float)(pfVar20[3] - 0.0027222223579883575);
          pfVar20[0x14] = 0.0;
          pfVar20[0x15] = 0.0;
          pfVar20[0x16] = 0.0;
          fVar2 = (float)(((float)(pfVar20[0x13] * pfVar20[10] +
                  (pfVar20[0x12] * pfVar20[9] + pfVar20[0x11] * *pfVar19))) * pfVar20[0x1d]);
          pfVar20[0x14] = (float)(fVar2 * *pfVar19 + pfVar20[0x14]);
          pfVar20[0x15] = (float)(fVar2 * pfVar20[9] + pfVar20[0x15]);
          pfVar20[0x16] = (float)(fVar2 * pfVar20[10] + pfVar20[0x16]);
          fVar2 = (float)(((float)(pfVar20[0x13] * pfVar20[0xd] +
                  (pfVar20[0x12] * pfVar20[0xc] + pfVar20[0xb] * pfVar20[0x11]))) * pfVar20[0x1e]);
          pfVar20[0x14] = (float)(fVar2 * pfVar20[0xb] + pfVar20[0x14]);
          pfVar20[0x15] = (float)(fVar2 * pfVar20[0xc] + pfVar20[0x15]);
          pfVar20[0x16] = (float)(fVar2 * pfVar20[0xd] + pfVar20[0x16]);
          fVar2 = (float)(((float)(pfVar20[0x13] * pfVar20[0x10] +
                  (pfVar20[0x12] * pfVar20[0xf] + pfVar20[0x11] * pfVar20[0xe]))) * pfVar20[0x1f]);
          pfVar20[0x14] = (float)(fVar2 * pfVar20[0xe] + pfVar20[0x14]);
          pfVar20[0x15] = (float)(fVar2 * pfVar20[0xf] + pfVar20[0x15]);
          pfVar20[0x16] = (float)(fVar2 * pfVar20[0x10] + pfVar20[0x16]);
          local_50 = (float)(pfVar20[0x15] * pfVar20[0x15] + pfVar20[0x14] * pfVar20[0x14] +
                     pfVar20[0x16] * pfVar20[0x16]);
          dVar21 = _CIsqrt((double)local_50);
          local_4c = ((int)LF_BITS_U((float)dVar21));
          if (1.52587890625E-5 < LF_BITS_F(local_4c)) {
            local_50 = (float)(1.0 / LF_BITS_F(local_4c));
            local_10 = (float)(local_50 * pfVar20[0x14]);
            local_c = (float)(local_50 * pfVar20[0x15]);
            local_8 = (float)(local_50 * pfVar20[0x16]);
            rotateorientation(pfVar19,&local_10,LF_BITS_F(local_4c));
          }
          objectwheelcollision2(local_44,iVar7);
          iVar7 = iVar7 + 1;
          pfVar20 = pfVar20 + 0x3a;
        } while (iVar7 < *(int *)((int)&g_object[0].unk_71ec + iVar15));
      }
      pfVar20 = local_40;
      iVar7 = 0;
      if (0 < *(int *)((int)&g_object[0].num_wheels + iVar15)) {
        pfVar19 = (float *)(&g_object[0].lf_raw[0x5f28] + iVar15);
        do {
          pfVar19[-1] = (float)(pfVar19[5] + pfVar19[-1]);
          *pfVar19 = (float)(pfVar19[6] + *pfVar19);
          pfVar19[1] = (float)(pfVar19[7] + pfVar19[1]);
          pfVar19[6] = (float)(pfVar19[6] - 0.0027222223579883575);
          iVar13 = *(int *)((int)(g_object[0].rot + 9) + (((int)LF_UINT(pfVar19[-0xc])) + (int)pfVar20 + 1) * 4
                           );
          iVar11 = *(int *)((int)(g_object[0].rot + 9) + (((int)LF_UINT(pfVar19[-0xb])) + (int)pfVar20 + 1) * 4
                           );
          LF_COPY(fVar2, pfVar19[-9]);
          local_50 = pfVar19[9];
          local_10 = (float)(pfVar19[-1] -
                     ((float)(local_50 * *(float *)((int)&g_object[0].rot[1].x + iVar15) +
                     ((float)(fVar2 * ((float)((&g_particle[0].pos.x)[iVar11 * 0x3c] -
                             (&g_particle[0].pos.x)[iVar13 * 0x3c])) +
                     (&g_particle[0].pos.x)[iVar13 * 0x3c])))));
          local_c = (float)(*pfVar19 -
                    ((float)(local_50 * *(float *)((int)&g_object[0].rot[1].y + iVar15) +
                    ((float)(fVar2 * ((float)((&g_particle[0].pos.y)[iVar11 * 0x3c] -
                            (&g_particle[0].pos.y)[iVar13 * 0x3c])) +
                    (&g_particle[0].pos.y)[iVar13 * 0x3c])))));
          local_8 = (float)(pfVar19[1] -
                    ((float)(local_50 * *(float *)((int)&g_object[0].rot[1].z + iVar15) +
                    ((float)(fVar2 * ((float)((&g_particle[0].pos.z)[iVar11 * 0x3c] -
                            (&g_particle[0].pos.z)[iVar13 * 0x3c])) +
                    (&g_particle[0].pos.z)[iVar13 * 0x3c])))));
          pfVar19[8] = (float)(local_8 * *(float *)((int)&g_object[0].rot[1].z + iVar15) +
                       (local_10 * *(float *)((int)&g_object[0].rot[1].x + iVar15) +
                       *(float *)((int)&g_object[0].rot[1].y + iVar15) * local_c));
          pfVar19[-3] = (float)(pfVar19[-4] + pfVar19[-3]);
          objectwheelcollision(local_44,iVar7);
          iVar7 = iVar7 + 1;
          pfVar19 = pfVar19 + 0x26;
        } while (iVar7 < *(int *)((int)&g_object[0].num_wheels + iVar15));
      }
      if (*(int *)((int)g_object[0].rot + iVar15 + -0x34) == 3) {
        objecttrainsimulation(local_44);
      }
      local_48 = (int *)0x0;
      if ((*(int *)((int)g_object[0].rot + iVar15 + -0x34) != 3) &&
         (local_4c = 0, 0 < *(int *)((int)(g_object[0].rot + 9) + iVar15))) {
        piVar16 = (int *)((int)(g_object[0].rot + 9) + iVar15 + 4);
        do {
          iVar7 = *piVar16;
          local_1c = (float)((&g_particle[0].pos.x)[iVar7 * 0x3c] -
                     *(float *)((int)&g_object[0].rot[-2].x + iVar15));
          local_18 = (float)((&g_particle[0].pos.y)[iVar7 * 0x3c] -
                     *(float *)((int)g_object[0].rot + iVar15 + -0x14));
          local_14 = (float)((&g_particle[0].pos.z)[iVar7 * 0x3c] -
                     *(float *)((int)g_object[0].rot + iVar15 + -0x10));
          local_50 = (float)(local_14 * local_14 + (local_18 * local_18 + local_1c * local_1c));
          dVar21 = _CIsqrt((double)local_50);
          local_50 = (float)dVar21;
          if ((64.0 <= local_50) ||
             (iVar7 = lineintersectlevel((float *)((int)&g_object[0].rot[-2].x + iVar15),
                                         &g_particle[0].pos.x + iVar7 * 0x3c,0), iVar7 == 0)) {
            iVar13 = lineintersectlevel((float *)((int)&g_object[0].rot[-1].x + iVar15),
                                        &g_particle[0].pos.x + *piVar16 * 0x3c,0);
            iVar7 = g_numofbonds;
            fVar2 = g_intersect.pos[2];
            if (iVar13 != 0) {
              iVar13 = *piVar16;
              local_10 = (float)(g_intersect.pos[0] - (&g_particle[0].pos.x)[iVar13 * 0x3c]);
              local_c = (float)(g_intersect.pos[1] - (&g_particle[0].pos.y)[iVar13 * 0x3c]);
              g_bond[g_numofbonds].type = 1;
              local_8 = (float)(fVar2 - (&g_particle[0].pos.z)[iVar13 * 0x3c]);
              g_bond[iVar7].particle_a = iVar13;
              fVar7 = g_intersect.normal[2];
              fVar17 = g_intersect.normal[1];
              fVar2 = g_intersect.normal[0];
              local_30 = (float)(g_intersect.normal[1] * local_c + g_intersect.normal[0] * local_10 +
                         g_intersect.normal[2] * local_8);
              g_bond[iVar7].rest_length = local_30;
              if (0.25 < local_30) {
                g_bond[iVar7].rest_length = 0.25;
              }
              g_numofbonds = g_numofbonds + 1;
              g_bond[iVar7].stiffness = 0.125;
              local_48 = (int *)0x1;
              g_bond[iVar7].damping = 0.25;
              g_bond[iVar7].mode = 0;
              g_bond[iVar7].friction_dir.x = fVar2;
              g_bond[iVar7].friction_dir.y = fVar17;
              g_bond[iVar7].friction_dir.z = fVar7;
              g_bond[iVar7].unk_00bc = 0;
              *(undefined4 *)&g_bond[iVar7].lf_raw[0xc0] = 0;
              *(undefined4 *)&g_bond[iVar7].lf_raw[0xc4] = 0;
              g_bond[iVar7].unk_0080 = 0x3f19999a;
            }
          }
          else {
            iVar7 = 0;
            if (g_intersect.unk_0058 != -1) {
              iVar7 = area_breakblock(g_intersect.unk_0054,g_intersect.unk_0058,g_intersect.unk_005c
                                      ,g_intersect.unk_0060,g_intersect.unk_0070,
                                      g_intersect.unk_0074,&g_particle[0].pos.x + *piVar16 * 0x3c
                                      ,&g_particle[0].vel.x + *piVar16 * 0x3c);
            }
            iVar13 = *piVar16;
            local_2c = (float)(g_intersect.normal[1] *
                       ((float)(g_intersect.pos[1] - (&g_particle[0].pos.y)[iVar13 * 0x3c])) +
                       (float)(g_intersect.normal[0] *
                       ((float)(g_intersect.pos[0] - (&g_particle[0].pos.x)[iVar13 * 0x3c]))) +
                       (float)(g_intersect.normal[2] *
                       ((float)(g_intersect.pos[2] - (&g_particle[0].pos.z)[iVar13 * 0x3c]))));
            local_3c = g_intersect.normal[0];
            local_38 = g_intersect.normal[1];
            local_34 = g_intersect.normal[2];
            local_10 = (float)((&g_particle[0].pos.x)[iVar13 * 0x3c] + g_yaxis[0] * 0.5);
            local_c = (float)(g_yaxis[1] * 0.5 + (&g_particle[0].pos.y)[iVar13 * 0x3c]);
            local_8 = (float)(g_yaxis[2] * 0.5 + (&g_particle[0].pos.z)[iVar13 * 0x3c]);
            iVar11 = lineintersectlevel(&local_10,&g_particle[0].pos.x + iVar13 * 0x3c,0);
            iVar13 = g_numofbonds;
            if (iVar11 != 0) {
              iVar11 = *piVar16;
              local_10 = (float)(g_intersect.pos[0] - (&g_particle[0].pos.x)[iVar11 * 0x3c]);
              local_c = (float)(g_intersect.pos[1] - (&g_particle[0].pos.y)[iVar11 * 0x3c]);
              local_8 = (float)(g_intersect.pos[2] - (&g_particle[0].pos.z)[iVar11 * 0x3c]);
              local_30 = (float)(g_intersect.normal[1] * local_c + g_intersect.normal[0] * local_10 +
                         g_intersect.normal[2] * local_8);
              if (local_30 < local_2c) {
                local_3c = g_intersect.normal[0];
                local_38 = g_intersect.normal[1];
                local_34 = g_intersect.normal[2];
                local_2c = local_30;
              }
            }
            iVar11 = *piVar16;
            g_bond[g_numofbonds].type = 1;
            g_bond[iVar13].particle_a = iVar11;
            g_bond[iVar13].rest_length = local_2c;
            if (0.25 < local_2c) {
              g_bond[iVar13].rest_length = 0.25;
            }
            g_bond[iVar13].stiffness = 0.125;
            g_bond[iVar13].damping = 0.25;
            if (iVar7 != 0) {
              g_bond[iVar13].stiffness = 0.0078125;
              g_bond[iVar13].damping = 0.03125;
            }
            g_numofbonds = g_numofbonds + 1;
            iVar7 = *(int *)((int)g_object[0].rot + iVar15 + -0x34);
            g_bond[iVar13].mode = 0;
            g_bond[iVar13].friction_dir.x = local_3c;
            g_bond[iVar13].friction_dir.y = local_38;
            g_bond[iVar13].friction_dir.z = local_34;
            g_bond[iVar13].unk_00bc = 0;
            *(undefined4 *)&g_bond[iVar13].lf_raw[0xc0] = 0;
            *(undefined4 *)&g_bond[iVar13].lf_raw[0xc4] = 0;
            g_bond[iVar13].unk_0080 = 0x3f19999a;
            if ((iVar7 == 2) && (0x14 < local_4c)) {
              applydamageobject(local_44,100);
            }
          }
          local_4c = local_4c + 1;
          piVar16 = piVar16 + 1;
        } while (local_4c < *(int *)((int)(g_object[0].rot + 9) + iVar15));
        if (local_48 != (int *)0x0) goto LAB_00457ea9;
      }
      *(undefined4 *)((int)&g_object[0].rot[-1].x + iVar15) =
           *(undefined4 *)((int)&g_object[0].rot[-2].x + iVar15);
      *(undefined4 *)((int)g_object[0].rot + iVar15 + -8) =
           *(undefined4 *)((int)g_object[0].rot + iVar15 + -0x14);
      *(undefined4 *)((int)g_object[0].rot + iVar15 + -4) =
           *(undefined4 *)((int)g_object[0].rot + iVar15 + -0x10);
    }
LAB_00457ea9:
    local_44 = local_44 + 1;
    local_40 = (float *)((int)local_40 + 0x208c);
    iVar15 = iVar15 + 0x8230;
    if (0x822fff < (int)local_40) {
      iVar15 = 0;
      local_44 = 0;
      do {
        if ((*(int *)((int)&((Object *)(g_object[0].rot + -5))->active + iVar15) != 0) &&
           (*(int *)((int)g_object[0].rot + iVar15 + -0x34) != 3)) {
          local_4c = 0;
          local_48 = &g_object[0].state;
          do {
            if (((((((local_4c != local_44) && (((Object *)(local_48 + -2))->active != 0)) &&
                   (*local_48 != 3)) &&
                  ((*(int *)((int)&g_object[0].unk_5be4 + iVar15) != local_4c &&
                   (local_48[0x16f7] != local_44)))) &&
                 (((LF_BITS_F(local_48[0x1f]) <= *(float *)((int)(g_object[0].rot + 7) + iVar15) &&
                   ((LF_BITS_F(local_48[0x20]) <= *(float *)((int)(g_object[0].rot + 7) + iVar15 + 4) &&
                    (LF_BITS_F(local_48[0x21]) <= *(float *)((int)(g_object[0].rot + 7) + iVar15 + 8))))
                   ) && (*(float *)((int)(g_object[0].rot + 6) + iVar15) <= LF_BITS_F(local_48[0x22]))))
                 ) && (((*(float *)((int)(g_object[0].rot + 6) + iVar15 + 4) <=
                         LF_BITS_F(local_48[0x23]) &&
                        (*(float *)((int)(g_object[0].rot + 6) + iVar15 + 8) <=
                         LF_BITS_F(local_48[0x24]))) &&
                       ((*(int *)((int)g_object[0].rot + iVar15 + -0x38) != 9 ||
                        (local_48[-1] != 10)))))) &&
               (0 < *(int *)((int)(g_object[0].rot + 9) + iVar15))) {
              local_40 = *(float **)((int)(g_object[0].rot + 9) + iVar15);
              piVar16 = (int *)((int)(g_object[0].rot + 9) + iVar15 + 4);
              do {
                iVar7 = *piVar16;
                iVar12 = lineintersectobject(local_4c,(float *)((int)&g_object[0].rot[-2].x + iVar15
                                                               ),
                                             &g_particle[0].pos.x + iVar7 * 0x3c,0);
                iVar11 = g_numofbonds;
                iVar13 = g_intersect.unk_0034;
                if (iVar12 != 0) {
                  local_10 = (float)(g_intersect.pos[0] - (&g_particle[0].pos.x)[iVar7 * 0x3c]);
                  local_c = (float)(g_intersect.pos[1] - (&g_particle[0].pos.y)[iVar7 * 0x3c]);
                  local_8 = (float)(g_intersect.pos[2] - (&g_particle[0].pos.z)[iVar7 * 0x3c]);
                  g_bond[g_numofbonds].type = 3;
                  g_bond[g_numofbonds].particle_a = iVar7;
                  g_bond[g_numofbonds].particle_b = iVar13;
                  fVar2 = g_intersect.unk_0044;
                  g_bond[g_numofbonds].unk_001c = g_intersect.unk_0038;
                  g_bond[g_numofbonds].unk_0020 = g_intersect.unk_003c;
                  g_bond[g_numofbonds].mass_ratio[0] = fVar2;
                  pBVar6 = g_bond + g_numofbonds;
                  g_numofbonds = g_numofbonds + 1;
                  pBVar6->mass_ratio[1] = g_intersect.unk_0048;
                  g_bond[iVar11].unk_0054 = g_intersect.unk_004c;
                  fVar3 = g_intersect.normal[2];
                  fVar17 = g_intersect.normal[1];
                  fVar2 = g_intersect.normal[0];
                  g_bond[iVar11].rest_length =
                       (float)(g_intersect.normal[1] * local_c + g_intersect.normal[0] * local_10 +
                       (float)(g_intersect.normal[2] * local_8));
                  g_bond[iVar11].stiffness = 0.0625;
                  g_bond[iVar11].damping = 0.25;
                  g_bond[iVar11].mode = 0;
                  g_bond[iVar11].friction_dir.x = fVar2;
                  g_bond[iVar11].friction_dir.y = fVar17;
                  g_bond[iVar11].friction_dir.z = fVar3;
                  g_bond[iVar11].unk_0080 = 0x3ecccccd;
                }
                piVar16 = piVar16 + 1;
                local_40 = (float *)((int)local_40 + -1);
              } while (local_40 != (float *)0x0);
            }
            local_4c = local_4c + 1;
            local_48 = local_48 + 0x208c;
          } while ((int)local_48 < ((int)SR_VA(0x7a1e608)));
        }
        local_44 = local_44 + 1;
        iVar15 = iVar15 + 0x8230;
      } while (iVar15 < 0x208c000);
      stoptimer(g_timerlist.unk_008c);
      return;
    }
  } while( true );
}
#undef g_objecttype
#undef g_object
#undef DAT_00460d18
#undef DAT_1992d668
#undef g_server
#undef g_game
#undef g_soundlist
#undef g_event
#undef g_particle
#undef g_city
#undef g_numofevents
#undef g_intersect
#undef g_timerlist
#undef g_track
#undef g_yaxis
#undef g_numofbonds
#undef g_bond
#undef g_timer
#undef stoptimer
#undef rotateorientation
#undef createevent_updateobject
#undef applydamageobject
#undef objectsimulation_heli
#undef lineintersectobject
#undef objecttrainsimulation
#undef objectsimulation_car
#undef area_breakblock
#undef lineintersectlevel
#undef objectwheelcollision2
#undef objectwheelcollision
#undef __security_check_cookie
#undef _ftol2
#undef _CIsqrt
#undef local_50
#undef local_4c
#undef local_48
#undef local_44
#undef local_40
#undef local_3c
#undef local_38
#undef local_34
#undef local_30
#undef local_2c
#undef local_28
#undef local_24
#undef local_20
#undef local_1c
#undef local_18
#undef local_14
#undef local_10
#undef local_c
#undef local_8
#undef local_4

