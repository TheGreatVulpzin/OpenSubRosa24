/* ai_playercar (RVA 0xe4d0): the driving of a car by the AI, rewritten by hand.
 *
 * Phase 6 (docs/plano.md). The first version is the lifted C, unchanged. Each step makes
 * it readable and faster and keeps the harness at 0 differences: the float math keeps its
 * order and its (float) rounding points (x87, 53-bit precision, as the original).
 *
 * The function keeps the symbol lf_0e4d0: tools/lift/lift.py sees the definition here
 * and lifts it no more, but keeps it in the table, the live hooks and the standalone server.
 */
#include "lift_support.h"
#include "lift_os.h"

/* Imports of the game (MSVCR100, KERNEL32...), resolved on the first call, as in the
 * lifted file. The indexes come from the lifted C. */
static void *lf_imports[64];
#define LF_IMPORT(i, dll, name) (lf_imports[i] ? lf_imports[i] : (lf_imports[i] = sr_import(dll, name)))
#define LF_IMPORT_ORD(i, dll, o) (lf_imports[i] ? lf_imports[i] : (lf_imports[i] = sr_import_ordinal(dll, o)))

#define g_player (*(Player (*)[256])SR_VA(0xf1edd00))
#define g_object (*(Object (*)[1024])SR_VA(0x5992600))
#define g_city (*(City *)SR_VA(0xe699220))
#define rand ((int (__cdecl *)(void))LF_IMPORT(6, "MSVCR100.DLL", "rand"))
#define ai_car_nextwaypoint ((void (__cdecl *)(uint))SR_FN(0x401610))
#define dotproduct ((float10 (__cdecl *)(float *, float *))SR_FN(0x40b850))
#define vectorlength ((float10 (__cdecl *)(float *))SR_FN(0x40b8c0))
#define normalizevector ((void (__cdecl *)(float *, float *))SR_FN(0x40b900))
#define vectordistance ((float10 (__cdecl *)(float *, float *))SR_FN(0x40baa0))
#define _CIsqrt lf_CIsqrt
#define local_54 (*(float *)(lf_frame + 0x1c))
#define local_50 (*(float *)(lf_frame + 0x20))
#define local_4c (*(uint *)(lf_frame + 0x24))
#define local_3c (*(float *)(lf_frame + 0x34))
#define local_38 (*(float *)(lf_frame + 0x38))
#define local_34 (*(float *)(lf_frame + 0x3c))
#define local_30 (*(float *)(lf_frame + 0x40))
#define local_28 (*(float *)(lf_frame + 0x48))
#define local_24 (*(float *)(lf_frame + 0x4c))
#define local_20 (*(float *)(lf_frame + 0x50))
#define local_1c (*(float *)(lf_frame + 0x54))
#define local_18 (*(float *)(lf_frame + 0x58))
#define local_14 (*(float *)(lf_frame + 0x5c))
#define local_10 (*(float *)(lf_frame + 0x60))
#define local_c (*(float *)(lf_frame + 0x64))
#define local_8 (*(float *)(lf_frame + 0x68))
#define local_4 (*(float *)(lf_frame + 0x6c))
void __cdecl lf_0e4d0(float param_1,int param_2)

{
  __declspec(align(16)) uchar lf_frame[0x70];
  float fVar1;
  float fVar2;
  float fVar3;
  float fVar4;
  int iVar5;
  int iVar6;
  int iVar7;
  int iVar8;
  float fVar9;
  float fVar10;
  float fVar11;
  Player *pPVar12;
  uint uVar13;
  int *piVar14;
  int iVar15;
  int iVar16;
  float *pfVar17;
  float10 fVar18;
  double dVar19;

















  float fVar13;
  
  LF_COPY(fVar13, param_1);
  if ((1 < g_object[param_2].gear) && (g_object[param_2].engine_rpm < 4.363323129986111)) {
    g_object[param_2].gear = g_object[param_2].gear + -1;
  }
  if ((g_object[param_2].gear < 4) && (8.726646259972222 < g_object[param_2].engine_rpm)) {
    g_object[param_2].gear = g_object[param_2].gear + 1;
  }
  iVar5 = g_object[param_2].gear;
  fVar1 = 1.0;
  if ((iVar5 == 1) || (iVar5 == 2)) {
    g_player[((int)LF_UINT(param_1))].input_axes[0] = 0.0;
  }
  else {
    g_player[((int)LF_UINT(param_1))].input_axes[0] = 1.0;
  }
  iVar16 = ((int)LF_UINT(param_1)) * 0x2408;
  if ((iVar5 == 1) || (iVar5 == 3)) {
    fVar1 = -1.0;
  }
  iVar5 = g_player[((int)LF_UINT(param_1))].ai_waypoint;
  g_player[((int)LF_UINT(param_1))].input_axes[2] = fVar1;
  iVar5 = iVar5 * 0x1c;
  iVar15 = *(int *)(&g_player[((int)LF_UINT(param_1))].lf_raw[0x1a58] + iVar5);
  if (iVar15 == -1) {
    return;
  }
  iVar6 = (&g_city.unk_1a010)[iVar15 * 0x13];
  iVar7 = (&g_city.unk_1a00c)[iVar15 * 0x13];
  if (*(int *)(&g_player[((int)LF_UINT(param_1))].lf_raw[0x1a5c] + iVar5) == 0) {
    local_18 = (&g_city.unk_0014)[iVar7 * 0x1a];
    local_14 = (&g_city.unk_0018)[iVar7 * 0x1a];
    local_10 = (&g_city.unk_001c)[iVar7 * 0x1a];
    local_30 = (&g_city.unk_0014)[iVar6 * 0x1a];
    fVar1 = (&g_city.unk_001c)[iVar6 * 0x1a];
  }
  else {
    local_18 = (&g_city.unk_0014)[iVar6 * 0x1a];
    local_14 = (&g_city.unk_0018)[iVar6 * 0x1a];
    local_10 = (&g_city.unk_001c)[iVar6 * 0x1a];
    local_30 = (&g_city.unk_0014)[iVar7 * 0x1a];
    fVar1 = (&g_city.unk_001c)[iVar7 * 0x1a];
  }
  fVar2 = g_object[param_2].pos.x;
  fVar3 = g_object[param_2].pos.y;
  local_1c = g_object[param_2].pos.z;
  local_50 = 16.0;
  if (g_player[((int)LF_UINT(param_1))].unk_1a3c != 0) {
    local_50 = 32.0;
  }
  local_50 = (float)(local_50 -
             ABS((float)(((float)(local_10 - local_1c)) * (&g_city.unk_1a034)[iVar15 * 0x13] +
                 ((&g_city.unk_1a02c)[iVar15 * 0x13] * ((float)(local_18 - fVar2)) +
                 ((float)(local_14 - fVar3)) * (&g_city.unk_1a030)[iVar15 * 0x13]))) * 0.75);
  if (local_50 < 0.0) {
    local_50 = 0.0;
  }
  iVar8 = *(int *)(&g_player[((int)LF_UINT(param_1))].lf_raw[0x1a5c] + iVar5);
  local_28 = (float)*(int *)(&g_player[((int)LF_UINT(param_1))].lf_raw[0x1a60] + iVar5) * 4.0;
  if (iVar8 == 0) {
    local_28 = (float)(local_28 + 2.0);
    fVar4 = local_28 * (&g_city.unk_1a02c)[iVar15 * 0x13];
    fVar10 = local_28 * (&g_city.unk_1a030)[iVar15 * 0x13];
    fVar9 = (&g_city.unk_1a034)[iVar15 * 0x13] * local_28;
    fVar11 = local_28 * (&g_city.unk_1a02c)[iVar15 * 0x13];
    local_28 = local_28 * (&g_city.unk_1a034)[iVar15 * 0x13];
    local_24 = (&g_city.unk_1a020)[iVar15 * 0x13] * local_50;
    local_20 = local_50 * (&g_city.unk_1a024)[iVar15 * 0x13];
    local_50 = local_50 * (&g_city.unk_1a028)[iVar15 * 0x13];
    local_c = (&g_city.unk_1a020)[iVar15 * 0x13];
    local_8 = (&g_city.unk_1a024)[iVar15 * 0x13];
    local_4 = (&g_city.unk_1a028)[iVar15 * 0x13];
  }
  else {
    local_28 = (float)(-2.0 - local_28);
    fVar4 = local_28 * (&g_city.unk_1a02c)[iVar15 * 0x13];
    fVar10 = local_28 * (&g_city.unk_1a030)[iVar15 * 0x13];
    fVar9 = (&g_city.unk_1a034)[iVar15 * 0x13] * local_28;
    fVar11 = local_28 * (&g_city.unk_1a02c)[iVar15 * 0x13];
    local_28 = local_28 * (&g_city.unk_1a034)[iVar15 * 0x13];
    local_50 = -local_50;
    local_24 = local_50 * (&g_city.unk_1a020)[iVar15 * 0x13];
    local_20 = local_50 * (&g_city.unk_1a024)[iVar15 * 0x13];
    local_50 = local_50 * (&g_city.unk_1a028)[iVar15 * 0x13];
    local_c = -(&g_city.unk_1a020)[iVar15 * 0x13];
    local_8 = -(&g_city.unk_1a024)[iVar15 * 0x13];
    local_4 = -(&g_city.unk_1a028)[iVar15 * 0x13];
  }
  local_10 = (float)(local_10 + fVar9);
  local_14 = (float)(fVar10 + local_14);
  local_18 = (float)(fVar4 + local_18);
  local_1c = (float)(local_50 + local_1c);
  local_20 = (float)(local_20 + fVar3);
  local_24 = (float)(local_24 + fVar2);
  local_28 = (float)(local_28 + fVar1);
  local_30 = (float)(fVar11 + local_30);
  if ((&g_city.unk_1a014)[iVar15 * 0x13] == 0) {
    if (iVar8 == 0) {
      fVar1 = -(float)(&g_city.unk_003c)[iVar6 * 0x1a];
    }
    else {
      fVar1 = (float)(&g_city.unk_0034)[iVar7 * 0x1a];
    }
  }
  else if (iVar8 == 0) {
    fVar1 = -(float)(&g_city.unk_0038)[iVar6 * 0x1a];
  }
  else {
    fVar1 = (float)(&g_city.unk_0030)[iVar7 * 0x1a];
  }
  local_30 = (float)((float)(fVar1 * 6.0) * (&g_city.unk_1a020)[iVar15 * 0x13] + local_30);
  local_28 = (float)(fVar1 * 6.0 * (&g_city.unk_1a028)[iVar15 * 0x13] + local_28);
  fVar1 = (float)(local_4 * ((float)(local_1c - local_10)) +
          (local_c * ((float)(local_24 - local_18)) + local_8 * ((float)(local_20 - local_14))));
  local_24 = (float)(fVar1 * local_c + local_18);
  local_20 = (float)(local_8 * fVar1 + local_14);
  local_1c = (float)(local_10 + local_4 * fVar1);
  local_3c = (float)(local_24 - g_object[param_2].pos.x);
  local_38 = (float)(local_20 - g_object[param_2].pos.y);
  local_34 = (float)(local_1c - g_object[param_2].pos.z);
  normalizevector(&local_3c,&local_3c);
  local_14 = (float)(local_3c * g_object[param_2].rot[2].z - local_34 * g_object[param_2].rot[2].x);
  local_54 = (float)(g_object[param_2].rot[1].z *
             ((float)(local_38 * g_object[param_2].rot[2].x - g_object[param_2].rot[2].y * local_3c)) +
             (((float)(local_34 * g_object[param_2].rot[2].y - local_38 * g_object[param_2].rot[2].z)) *
             g_object[param_2].rot[1].x + g_object[param_2].rot[1].y * local_14));
  if ((float)(local_34 * g_object[param_2].rot[2].z +
      (local_3c * g_object[param_2].rot[2].x + g_object[param_2].rot[2].y * local_38)) <= 0.7070000171661377) {
LAB_0040eaa5:
    if (-1.0 <= local_54) {
      if (1.0 < local_54) {
        local_54 = 1.0;
      }
    }
    else {
      local_54 = -1.0;
    }
  }
  else if (0.0 <= local_54) {
    if (local_54 <= 0.0) goto LAB_0040eaa5;
    local_54 = 1.0;
  }
  else {
    local_54 = -1.0;
  }
  g_player[((int)LF_UINT(param_1))].input_axes[1] = (float)(local_54 + g_player[((int)LF_UINT(param_1))].input_axes[1]);
  local_18 = (float)(local_30 - g_object[param_2].pos.x);
  local_10 = (float)(local_28 - g_object[param_2].pos.z);
  iVar15 = (&g_city.unk_1a00c)
           [(*(uint *)(&g_player[((int)LF_UINT(param_1))].lf_raw[0x1a5c] + iVar5) ^ 1) + iVar15 * 0x13];
  iVar6 = *(int *)(&g_player[((int)LF_UINT(param_1))].lf_raw[0x1a58] + iVar5);
  local_50 = (float)(local_10 * local_4 + (local_8 * 0.0 + local_18 * local_c));
  local_4c = (uint)(iVar6 == (&g_city.unk_0024)[iVar15 * 0x1a]);
  if (iVar6 == (&g_city.unk_0028)[iVar15 * 0x1a]) {
    local_4c = 2;
  }
  if (iVar6 == (&g_city.unk_002c)[iVar15 * 0x1a]) {
    local_4c = 3;
  }
  if ((iVar15 == 4) && (local_4c == 3)) {
    local_50 = (float)(local_50 - 14.0);
  }
  if ((local_50 < 6.0) &&
     (((&g_city.unk_0050)[iVar15 * 0x1a + local_4c] == 0 || (g_player[((int)LF_UINT(param_1))].unk_1a3c != 0)))
     ) {
    *(undefined4 *)(&g_player[((int)LF_UINT(param_1))].lf_raw[0x1a58] + iVar5) =
         *(undefined4 *)(&g_player[((int)LF_UINT(param_1))].lf_raw[0x1a74] + iVar5);
    iVar5 = g_player[((int)LF_UINT(param_1))].ai_waypoint * 0x1c;
    *(undefined4 *)(&g_player[((int)LF_UINT(param_1))].lf_raw[0x1a5c] + iVar5) =
         *(undefined4 *)(&g_player[((int)LF_UINT(param_1))].lf_raw[0x1a78] + iVar5);
    iVar5 = g_player[((int)LF_UINT(param_1))].ai_waypoint * 0x1c;
    *(undefined4 *)(&g_player[((int)LF_UINT(param_1))].lf_raw[0x1a60] + iVar5) =
         *(undefined4 *)(&g_player[((int)LF_UINT(param_1))].lf_raw[0x1a7c] + iVar5);
    ai_car_nextwaypoint(((uint)LF_UINT(param_1)));
  }
  iVar5 = g_player[((int)LF_UINT(param_1))].unk_2154;
  if (((iVar5 != -1) && (g_player[((int)LF_UINT(param_1))].unk_2158 == (&g_city.unk_0040)[iVar5 * 0x1a])) &&
     (fVar18 = (float10)vectordistance(&g_object[param_2].pos.x,&g_city.unk_0014 + iVar5 * 0x1a),
     (float10)32.0 < fVar18)) {
    (&g_city.unk_0040)[iVar5 * 0x1a] = (&g_city.unk_0040)[iVar5 * 0x1a] + 1;
    (&g_city.unk_0048)[iVar5 * 0x1a] = 0;
    g_player[((int)LF_UINT(param_1))].unk_2154 = -1;
  }
  local_3c = (float)(local_24 - g_object[param_2].pos.x);
  local_38 = (float)(local_20 - g_object[param_2].pos.y);
  local_34 = (float)(local_1c - g_object[param_2].pos.z);
  normalizevector(&local_3c,&local_3c);
  pPVar12 = g_player + ((int)LF_UINT(param_1));
  fVar1 = local_3c * g_object[param_2].rot[2].x;
  fVar1 = (float)(ABS((float)(local_34 * g_object[param_2].rot[2].z + (g_object[param_2].rot[2].y * local_38 + fVar1)))
          * ABS((float)(local_38 * g_object[param_2].rot[2].y + fVar1 +
                local_34 * g_object[param_2].rot[2].z)) * 0.75 + 0.25);
  fVar2 = fVar1 * 20.0;
  if ((pPVar12->unk_1a3c != 0) &&
     (fVar2 = fVar1 * 30.0,
     *(int *)(iVar16 + ((int)&g_player[0].lf_raw[0x1a80]) + g_player[((int)LF_UINT(param_1))].ai_waypoint * 0x1c) == 0)) {
    fVar2 = fVar1 * 40.0;
  }
  param_1 = (float)(fVar2 / 60.0);
  local_54 = (float)((local_50 * 0.5) / 60.0);
  if (local_50 < 2.0) {
    local_54 = 0.0;
  }
  if ((&g_city.unk_0050)[iVar15 * 0x1a + local_4c] == 0) {
    if (*(int *)(iVar16 + ((int)&g_player[0].lf_raw[0x1a80]) + g_player[((int)LF_UINT(fVar13))].ai_waypoint * 0x1c) == 0) {
      local_54 = param_1;
    }
    else {
      local_54 = (float)(((local_50 + 16.0) * 0.5) / 60.0);
    }
  }
  if (pPVar12->unk_1a3c != 0) {
    local_54 = (float)(local_54 * 4.0);
  }
  if (local_54 < param_1) {
    param_1 = local_54;
  }
  if (0 < g_player[((int)LF_UINT(fVar13))].unk_1a40) {
    g_player[((int)LF_UINT(fVar13))].unk_1a40 = g_player[((int)LF_UINT(fVar13))].unk_1a40 + -1;
  }
  iVar5 = 0;
  pfVar17 = &g_object[0].pos.z;
  do {
    LF_COPY(fVar1, param_1);
    if ((iVar5 != param_2) && (LF_UINT(pfVar17[-0xb]) != 0x0U)) {
      local_3c = (float)(((vec3 *)(pfVar17 + -2))->x - g_object[param_2].pos.x);
      local_38 = (float)(pfVar17[-1] - g_object[param_2].pos.y);
      local_34 = (float)(*pfVar17 - g_object[param_2].pos.z);
      if ((ABS(local_3c) < 64.0) && (ABS(local_34) < 64.0)) {
        fVar2 = ABS((float)(g_object[param_2].rot[0].z * local_34 +
                    (g_object[param_2].rot[0].x * local_3c + local_38 * g_object[param_2].rot[0].y)));
        if ((((g_player[((int)LF_UINT(fVar13))].unk_1a3c != 0) && (g_player[((int)LF_UINT(fVar13))].unk_1a40 < 1)) &&
            (2.0 < (float)(local_4 * local_34 + (local_c * local_3c + local_8 * local_38)))) && (fVar2 < 2.0))
        {
          uVar13 = rand();
          if ((uVar13 & 1) == 0) {
            piVar14 = (int *)(iVar16 + ((int)&g_player[0].lf_raw[0x1a60]) + g_player[((int)LF_UINT(fVar13))].ai_waypoint * 0x1c);
            iVar15 = *piVar14;
            if (1 < iVar15) goto LAB_0040ef80;
LAB_0040efa2:
            iVar15 = iVar15 + 1;
          }
          else {
LAB_0040ef80:
            iVar15 = *(int *)(iVar16 + ((int)&g_player[0].lf_raw[0x1a60]) + g_player[((int)LF_UINT(fVar13))].ai_waypoint * 0x1c);
            piVar14 = (int *)(iVar16 + ((int)&g_player[0].lf_raw[0x1a60]) + g_player[((int)LF_UINT(fVar13))].ai_waypoint * 0x1c);
            if (iVar15 < -1) goto LAB_0040efa2;
            iVar15 = iVar15 + -1;
          }
          *piVar14 = iVar15;
          if (2 < *(int *)(iVar16 + ((int)&g_player[0].lf_raw[0x1a60]) + g_player[((int)LF_UINT(fVar13))].ai_waypoint * 0x1c)) {
            *(undefined4 *)(iVar16 + ((int)&g_player[0].lf_raw[0x1a60]) + g_player[((int)LF_UINT(fVar13))].ai_waypoint * 0x1c) = 2;
          }
          if (*(int *)(iVar16 + ((int)&g_player[0].lf_raw[0x1a60]) + g_player[((int)LF_UINT(fVar13))].ai_waypoint * 0x1c) < -2) {
            *(undefined4 *)(iVar16 + ((int)&g_player[0].lf_raw[0x1a60]) + g_player[((int)LF_UINT(fVar13))].ai_waypoint * 0x1c) =
                 0xfffffffe;
          }
          g_player[((int)LF_UINT(fVar13))].unk_1a40 = 0x3c;
        }
        if ((2.0 < (float)(local_34 * local_4 + (local_38 * local_8 + local_c * local_3c))) && (fVar2 < 2.0)) {
          fVar18 = (float10)dotproduct(&g_object[param_2].rot[2].x,pfVar17 + 10);
          if ((float10)0.0 < fVar18) {
            fVar18 = (float10)vectordistance(&g_object[param_2].pos.x,(float *)(pfVar17 + -2));
            local_50 = (float)(fVar18 - (float10)8.0);
            if (local_50 < 0.0) {
              local_50 = 0.0;
            }
            if ((g_player[((int)LF_UINT(fVar13))].unk_1a3c != 0) && (local_50 < 16.0)) {
              local_50 = 16.0;
            }
            fVar18 = (float10)vectorlength(pfVar17 + 0xd);
            fVar1 = (float)(fVar18 + (float10)((float)((local_50 * 0.5) / 60.0)));
            if (fVar1 < param_1) goto LAB_0040f118;
          }
          LF_COPY(fVar1, param_1);
        }
      }
    }
LAB_0040f118:
    param_1 = fVar1;
    iVar5 = iVar5 + 1;
    pfVar17 = pfVar17 + 0x208c;
  } while ((int)pfVar17 < ((int)SR_VA(0x7a1e62c)));
  iVar5 = g_player[((int)LF_UINT(fVar13))].unk_2160;
  fVar1 = (float)(g_object[param_2].vel.z * g_object[param_2].rot[2].z +
          (g_object[param_2].rot[2].y * g_object[param_2].vel.y +
          g_object[param_2].vel.x * g_object[param_2].rot[2].x)) + param_1;
  local_54 = (float)(fVar1 * 3.0);
  if (iVar5 < 0xb4) {
    fVar2 = g_object[param_2].vel.y;
    fVar3 = g_object[param_2].vel.x;
    fVar4 = g_object[param_2].vel.z;
    dVar19 = _CIsqrt((double)((float)(fVar4 * fVar4 + (fVar3 * fVar3 + fVar2 * fVar2))));
    if (((float)dVar19 < 0.03333333507180214) && (0.06666667014360428 < fVar1)) {
      g_player[((int)LF_UINT(fVar13))].unk_2160 = iVar5 + 1;
      goto LAB_0040f22c;
    }
  }
  else {
    local_54 = 1.0;
    g_player[((int)LF_UINT(fVar13))].input_axes[2] = 1.0;
    g_player[((int)LF_UINT(fVar13))].input_axes[0] = 2.0;
    g_player[((int)LF_UINT(fVar13))].input_axes[1] = 0.0;
    if (0x117 < iVar5) {
      local_54 = -1.0;
    }
    g_player[((int)LF_UINT(fVar13))].unk_2160 = iVar5 + 1;
    if (iVar5 + 1 < 0x140) goto LAB_0040f22c;
  }
  g_player[((int)LF_UINT(fVar13))].unk_2160 = 0;
LAB_0040f22c:
  if (local_54 < 0.0) {
    local_54 = (float)(local_54 * 4.0);
  }
  if (-1.0 <= local_54) {
    if (local_54 <= 1.0) {
      g_player[((int)LF_UINT(fVar13))].input_axes[3] = local_54;
      return;
    }
    g_player[((int)LF_UINT(fVar13))].input_axes[3] = 1.0;
    return;
  }
  g_player[((int)LF_UINT(fVar13))].input_axes[3] = -1.0;
  return;
}
#undef g_player
#undef g_object
#undef g_city
#undef rand
#undef ai_car_nextwaypoint
#undef dotproduct
#undef vectorlength
#undef normalizevector
#undef vectordistance
#undef _CIsqrt
#undef local_54
#undef local_50
#undef local_4c
#undef local_3c
#undef local_38
#undef local_34
#undef local_30
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

