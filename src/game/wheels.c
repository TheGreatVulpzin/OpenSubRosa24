/* objectwheelsimulation2 (RVA 0x22f90): the wheels of an object: suspension, contact and spin, rewritten by hand.
 *
 * Phase 6 (docs/plano.md). The first version is the lifted C, unchanged. Each step makes
 * it readable and faster and keeps the harness at 0 differences: the float math keeps its
 * order and its (float) rounding points (x87, 53-bit precision, as the original).
 *
 * The function keeps the symbol lf_22f90: tools/lift/lift.py sees the definition here
 * and lifts it no more, but keeps it in the table, the live hooks and the standalone server.
 */
#include "lift_support.h"
#include "lift_os.h"

#define g_object (*(Object (*)[1024])SR_VA(0x5992600))
#define DAT_00460d18 (*(uint *)SR_VA(0x460d18))
#define DAT_00603dd0 (*(int *)SR_VA(0x603dd0))
#define DAT_00583dd0 (*(undefined4 *)SR_VA(0x583dd0))
#define DAT_00583dd4 (*(undefined4 *)SR_VA(0x583dd4))
#define DAT_00583dd8 (*(undefined4 *)SR_VA(0x583dd8))
#define DAT_00583de0 (*(undefined4 *)SR_VA(0x583de0))
#define DAT_00583de4 (*(undefined4 *)SR_VA(0x583de4))
#define g_particle (*(Particle (*)[4096])SR_VA(0xb7e7200))
#define DAT_00583de8 (*(undefined4 *)SR_VA(0x583de8))
#define rotatevector ((void (__cdecl *)(float *, float *, float))SR_FN(0x40bc50))
#define getobjectrotation3 ((void (__cdecl *)(float *, int))SR_FN(0x4225e0))
#define __security_check_cookie(x) ((void)0)
#define local_78 (*(float *)(lf_frame + 0x18))
#define local_74 (*(float *)(lf_frame + 0x1c))
#define local_70 (*(float *)(lf_frame + 0x20))
#define local_6c (*(float *)(lf_frame + 0x24))
#define local_68 (*(float *)(lf_frame + 0x28))
#define local_64 (*(float *)(lf_frame + 0x2c))
#define local_60 (*(float *)(lf_frame + 0x30))
#define local_5c (*(float *)(lf_frame + 0x34))
#define local_58 (*(float *)(lf_frame + 0x38))
#define local_54 (*(float *)(lf_frame + 0x3c))
#define local_50 (*(int *)(lf_frame + 0x40))
#define local_4c (*(float *)(lf_frame + 0x44))
#define local_48 (*(float *)(lf_frame + 0x48))
#define local_44 (*(int *)(lf_frame + 0x4c))
#define local_40 (*(int *)(lf_frame + 0x50))
#define local_3c (*(int *)(lf_frame + 0x54))
#define local_38 (*(int *)(lf_frame + 0x58))
#define local_34 (*(float *)(lf_frame + 0x5c))
#define local_30 (*(float *)(lf_frame + 0x60))
#define local_2c (*(float *)(lf_frame + 0x64))
#define local_10 (*(float *)(lf_frame + 0x80))
#define local_c (*(float *)(lf_frame + 0x84))
#define local_8 (*(float *)(lf_frame + 0x88))
#define local_4 (*(uint *)(lf_frame + 0x8c))
void __cdecl lf_22f90(void)

{
  __declspec(align(16)) uchar lf_frame[0x90];
  float fVar1;
  float fVar2;
  float fVar3;
  int iVar4;
  int iVar5;
  int iVar6;
  float fVar7;
  float fVar8;
  float fVar9;
  float fVar10;
  float fVar11;
  undefined4 *puVar12;
  float *pfVar13;
  int iVar14;
  int iVar15;
  int iVar16;
  float fVar17;
























  
  local_4 = DAT_00460d18 ^ (uint)&local_78;
  local_44 = 0;
  iVar16 = 0;
  local_40 = 0;
  do {
    if (*(int *)((int)&((Object *)(g_object[0].rot + -5))->active + iVar16) != 0) {
      iVar14 = 0;
      *(undefined4 *)((int)&g_object[0].engine_accel + iVar16) = 0;
      if (0 < *(int *)((int)&g_object[0].num_wheels + iVar16)) {
        puVar12 = (undefined4 *)(&g_object[0].lf_raw[0x5f34] + iVar16);
        do {
          puVar12[-8] = 0;
          iVar14 = iVar14 + 1;
          puVar12[-1] = puVar12[2];
          *puVar12 = puVar12[3];
          puVar12[1] = puVar12[4];
          puVar12[10] = 0;
          puVar12 = puVar12 + 0x26;
        } while (iVar14 < *(int *)((int)&g_object[0].num_wheels + iVar16));
      }
      local_50 = 0;
      if (0 < *(int *)((int)&g_object[0].num_wheels + iVar16)) {
        pfVar13 = (float *)(&g_object[0].lf_raw[0x5f7c] + iVar16);
        do {
          iVar14 = *(int *)((int)(g_object[0].rot + 9) + (((int)LF_UINT(pfVar13[-0x21])) + local_40 + 1) * 4);
          iVar4 = *(int *)((int)(g_object[0].rot + 9) + (((int)LF_UINT(pfVar13[-0x20])) + local_40 + 1) * 4);
          if (DAT_00603dd0 == 0) {
            local_58 = (float)((&g_particle[0].unk_0008)[iVar4 * 0x3c] * pfVar13[-0x1e] +
                       (&g_particle[0].unk_0008)[iVar14 * 0x3c] * pfVar13[-0x1f]);
            local_54 = (float)(local_58 + pfVar13[-0x1d]);
            local_58 = (float)(local_58 / local_54);
            local_54 = (float)(pfVar13[-0x1d] / local_54);
            pfVar13[-3] = local_58;
            pfVar13[-2] = local_54;
            LF_COPY(fVar17, pfVar13[-0x1e]);
            LF_COPY(fVar1, pfVar13[-0xc]);
            local_6c = pfVar13[-0xb];
            local_74 = (float)(local_6c *
                       ((float)(((float)(((float)((&g_particle[0].unk_0014)[iVar14 * 0x3c] +
                         fVar17 * ((float)((&g_particle[0].unk_0014)[iVar4 * 0x3c] -
                                  (&g_particle[0].unk_0014)[iVar14 * 0x3c])))) - pfVar13[-0x16])) +
                       fVar1 * *(float *)((int)&g_object[0].rot[1].x + iVar16))));
            local_70 = (float)(((float)(*(float *)((int)&g_object[0].rot[1].y + iVar16) * fVar1 +
                       ((float)(((float)(((float)((&g_particle[0].unk_0018)[iVar4 * 0x3c] -
                         (&g_particle[0].unk_0018)[iVar14 * 0x3c])) * fVar17 +
                        (&g_particle[0].unk_0018)[iVar14 * 0x3c])) - pfVar13[-0x15])))) * local_6c);
            local_6c = (float)(local_6c *
                       ((float)(fVar1 * *(float *)((int)&g_object[0].rot[1].z + iVar16) +
                       ((float)(((float)(fVar17 * ((float)((&g_particle[0].unk_001c)[iVar4 * 0x3c] -
                                  (&g_particle[0].unk_001c)[iVar14 * 0x3c])) +
                        (&g_particle[0].unk_001c)[iVar14 * 0x3c])) - pfVar13[-0x14])))));
            pfVar13[-1] = local_74;
            *pfVar13 = local_70;
            pfVar13[1] = local_6c;
          }
          else {
            local_58 = pfVar13[-3];
            local_54 = pfVar13[-2];
            local_74 = pfVar13[-1];
            local_70 = *pfVar13;
            local_6c = pfVar13[1];
          }
          fVar17 = -pfVar13[-10];
          fVar1 = (float)(pfVar13[-10] * pfVar13[-0x1f]);
          fVar2 = (float)(pfVar13[-10] * pfVar13[-0x1e]);
          local_68 = (float)(fVar2 * LF_BITS_F((&DAT_00583dd0)[iVar4 * 8]) +
                     ((float)(fVar1 * LF_BITS_F((&DAT_00583dd0)[iVar14 * 8]) + fVar17 * pfVar13[-0x13])));
          local_64 = (float)(LF_BITS_F((&DAT_00583dd4)[iVar4 * 8]) * fVar2 +
                     ((float)(LF_BITS_F((&DAT_00583dd4)[iVar14 * 8]) * fVar1 + (float)(pfVar13[-0x12] * fVar17))));
          fVar17 = (float)(fVar2 * LF_BITS_F((&DAT_00583dd8)[iVar4 * 8]) +
                   ((float)(fVar1 * LF_BITS_F((&DAT_00583dd8)[iVar14 * 8]) + (float)(fVar17 * pfVar13[-0x11]))));
          fVar1 = (float)(local_68 + local_74);
          fVar2 = (float)(local_64 + local_70);
          fVar3 = (float)(fVar17 + local_6c);
          fVar7 = -((float)(fVar3 * *(float *)((int)&g_object[0].rot[1].z + iVar16) +
                   (fVar2 * *(float *)((int)&g_object[0].rot[1].y + iVar16) +
                   fVar1 * *(float *)((int)&g_object[0].rot[1].x + iVar16))));
          fVar1 = (float)(fVar7 * *(float *)((int)&g_object[0].rot[1].x + iVar16) + fVar1);
          fVar2 = (float)(fVar7 * *(float *)((int)&g_object[0].rot[1].y + iVar16) + fVar2);
          fVar3 = (float)(fVar7 * *(float *)((int)&g_object[0].rot[1].z + iVar16) + fVar3);
          pfVar13[-0x10] = (float)(fVar1 * local_58 + pfVar13[-0x10]);
          pfVar13[-0xf] = (float)(fVar2 * local_58 + pfVar13[-0xf]);
          pfVar13[-0xe] = (float)(fVar3 * local_58 + pfVar13[-0xe]);
          local_78 = -local_54;
          fVar7 = (float)(local_78 * pfVar13[-0x1f]);
          *(float *)&((&DAT_00583de0)[iVar14 * 8]) = (float)(fVar7 * fVar1 + LF_BITS_F((&DAT_00583de0)[iVar14 * 8]));
          *(float *)&((&DAT_00583de4)[iVar14 * 8]) = (float)(fVar7 * fVar2 + LF_BITS_F((&DAT_00583de4)[iVar14 * 8]));
          *(float *)&((&DAT_00583de8)[iVar14 * 8]) = (float)(fVar7 * fVar3 + LF_BITS_F((&DAT_00583de8)[iVar14 * 8]));
          fVar7 = (float)(local_78 * pfVar13[-0x1e]);
          *(float *)&((&DAT_00583de0)[iVar4 * 8]) = (float)(LF_BITS_F((&DAT_00583de0)[iVar4 * 8]) + fVar7 * fVar1);
          *(float *)&((&DAT_00583de4)[iVar4 * 8]) = (float)(LF_BITS_F((&DAT_00583de4)[iVar4 * 8]) + fVar7 * fVar2);
          *(float *)&((&DAT_00583de8)[iVar4 * 8]) = (float)(LF_BITS_F((&DAT_00583de8)[iVar4 * 8]) + fVar3 * fVar7);
          fVar1 = (float)(pfVar13[-9] * 0.375);
          local_74 = (float)(fVar1 * local_74);
          local_70 = (float)(fVar1 * local_70);
          local_6c = (float)(fVar1 * local_6c);
          local_60 = pfVar13[-9];
          local_68 = (float)(local_60 * local_68);
          local_64 = (float)(local_60 * local_64);
          local_60 = (float)(local_60 * fVar17);
          local_8 = (float)(((float)(local_60 + local_6c)) * *(float *)((int)&g_object[0].rot[1].z + iVar16) +
                    (((float)(local_64 + local_70)) * *(float *)((int)&g_object[0].rot[1].y + iVar16) +
                    ((float)(local_68 + local_74)) * *(float *)((int)&g_object[0].rot[1].x + iVar16)));
          local_10 = (float)(local_8 * *(float *)((int)&g_object[0].rot[1].x + iVar16));
          local_c = (float)(local_8 * *(float *)((int)&g_object[0].rot[1].y + iVar16));
          local_8 = (float)(local_8 * *(float *)((int)&g_object[0].rot[1].z + iVar16));
          pfVar13[-0x10] = (float)(local_10 * local_58 + pfVar13[-0x10]);
          pfVar13[-0xf] = (float)(local_c * local_58 + pfVar13[-0xf]);
          pfVar13[-0xe] = (float)(pfVar13[-0xe] + local_8 * local_58);
          local_5c = (float)(local_78 * pfVar13[-0x1f]);
          *(float *)&((&DAT_00583de0)[iVar14 * 8]) = (float)(local_5c * local_10 + LF_BITS_F((&DAT_00583de0)[iVar14 * 8]));
          *(float *)&((&DAT_00583de4)[iVar14 * 8]) = (float)(local_5c * local_c + LF_BITS_F((&DAT_00583de4)[iVar14 * 8]));
          *(float *)&((&DAT_00583de8)[iVar14 * 8]) = (float)(local_5c * local_8 + LF_BITS_F((&DAT_00583de8)[iVar14 * 8]));
          local_50 = local_50 + 1;
          local_78 = pfVar13[-0x1e] * local_78;
          *(float *)&((&DAT_00583de0)[iVar4 * 8]) = (float)(LF_BITS_F((&DAT_00583de0)[iVar4 * 8]) + local_78 * local_10);
          *(float *)&((&DAT_00583de4)[iVar4 * 8]) = (float)(LF_BITS_F((&DAT_00583de4)[iVar4 * 8]) + local_78 * local_c);
          *(float *)&((&DAT_00583de8)[iVar4 * 8]) = (float)(local_78 * local_8 + LF_BITS_F((&DAT_00583de8)[iVar4 * 8]));
          pfVar13 = pfVar13 + 0x26;
        } while (local_50 < *(int *)((int)&g_object[0].num_wheels + iVar16));
      }
      local_50 = 0;
      if (0 < *(int *)((int)&g_object[0].unk_71ec + iVar16)) {
        do {
          iVar4 = local_40;
          iVar14 = local_50;
          local_34 = *(float *)((int)&g_object[0].rot[0].x + iVar16);
          local_30 = *(float *)((int)&g_object[0].rot[0].y + iVar16);
          local_2c = *(float *)((int)&g_object[0].rot[0].z + iVar16);
          if (local_50 == 0) {
            fVar17 = *(float *)((int)&g_object[0].unk_5f20 + iVar16);
LAB_0042360a:
            rotatevector(&local_34,(float *)((int)&g_object[0].rot[1].x + iVar16),fVar17);
          }
          else if (local_50 == 1) {
            fVar17 = *(float *)((int)&g_object[0].unk_5fb8 + iVar16);
            goto LAB_0042360a;
          }
          iVar14 = iVar14 * 0xe8;
          iVar15 = iVar14 + iVar16;
          local_78 = -((float)(*(float *)(&g_object[0].lf_raw[0x724c] + iVar15) * local_2c +
                      (*(float *)(&g_object[0].lf_raw[0x7244] + iVar15) * local_34 +
                      local_30 * *(float *)(&g_object[0].lf_raw[0x7248] + iVar15))));
          local_74 = (float)(*(float *)(&g_object[0].lf_raw[0x7244] + iVar15) + local_78 * local_34);
          local_70 = (float)(*(float *)(&g_object[0].lf_raw[0x7248] + iVar15) + local_78 * local_30);
          local_6c = (float)(local_2c * local_78 + *(float *)(&g_object[0].lf_raw[0x724c] + iVar15));
          local_68 = (float)((float)(((float)(local_30 * *(float *)(iVar14 + ((int)&g_object[0].lf_raw[0x721c]) + iVar16) -
                     local_2c * *(float *)(iVar14 + ((int)&g_object[0].lf_raw[0x7218]) + iVar16))) * 0.25) - local_74 * 0.5);
          local_64 = (float)((float)(((float)(*(float *)(&g_object[0].lf_raw[0x7214] + iVar15) * local_2c -
                     local_34 * *(float *)(&g_object[0].lf_raw[0x721c] + iVar15))) * 0.25) -
                     local_70 * 0.5);
          local_60 = (float)(((float)(*(float *)(&g_object[0].lf_raw[0x7218] + iVar15) * local_34 -
                     *(float *)(&g_object[0].lf_raw[0x7214] + iVar15) * local_30)) * 0.25 -
                     local_6c * 0.5);
          getobjectrotation3(&local_10,local_44);
          fVar1 = -((float)(local_2c * local_8 + (local_c * local_30 + local_34 * local_10)));
          fVar17 = *(float *)(&g_object[0].lf_raw[0x725c] + iVar15);
          *(float *)(&g_object[0].lf_raw[0x7250] + iVar15) =
               (float)(((float)(fVar17 * ((float)(local_68 + ((float)(fVar1 * local_34 + local_10)) * 0.5)) + 0.0)) * 0.5 +
               *(float *)(&g_object[0].lf_raw[0x7250] + iVar15));
          *(float *)(&g_object[0].lf_raw[0x7254] + iVar15) =
               (float)(((float)(((float)(((float)(fVar1 * local_30 + local_c)) * 0.5 + local_64)) * fVar17 + 0.0)) * 0.5 +
               *(float *)(&g_object[0].lf_raw[0x7254] + iVar15));
          *(float *)(&g_object[0].lf_raw[0x7258] + iVar15) =
               (float)(((float)(fVar17 * ((float)(((float)(fVar1 * local_2c + local_8)) * 0.5 + local_60)) + 0.0)) * 0.5 +
               *(float *)(&g_object[0].lf_raw[0x7258] + iVar15));
          fVar17 = *(float *)(&g_object[0].lf_raw[0x7290] + iVar15);
          iVar14 = *(int *)((int)(g_object[0].rot + 9) +
                           (*(int *)(&g_object[0].lf_raw[0x7280] + iVar15) + iVar4 + 1) * 4);
          iVar5 = *(int *)((int)(g_object[0].rot + 9) +
                          (*(int *)(&g_object[0].lf_raw[0x7284] + iVar15) + iVar4 + 1) * 4);
          iVar6 = *(int *)((int)(g_object[0].rot + 9) +
                          (*(int *)(&g_object[0].lf_raw[0x728c] + iVar15) + iVar4 + 1) * 4);
          local_3c = iVar14 * 0xf0;
          iVar4 = *(int *)((int)(g_object[0].rot + 9) +
                          (*(int *)(&g_object[0].lf_raw[0x7288] + iVar15) + iVar4 + 1) * 4);
          fVar1 = *(float *)(&g_object[0].lf_raw[0x7294] + iVar15);
          fVar2 = *(float *)(&g_object[0].lf_raw[0x7298] + iVar15);
          local_38 = iVar6 * 0xf0;
          fVar3 = *(float *)(&g_object[0].lf_raw[0x729c] + iVar15);
          fVar8 = (float)(((float)(((float)((float)((float)(fVar1 * (&g_particle[0].unk_0014)[iVar5 * 0x3c] +
                    fVar17 * (&g_particle[0].unk_0014)[iVar14 * 0x3c]) +
                    fVar2 * (&g_particle[0].unk_0014)[iVar4 * 0x3c]) +
                   fVar3 * (&g_particle[0].unk_0014)[iVar6 * 0x3c])) -
                  *(float *)((int)&g_object[0].rot[1].x + iVar16) * 0.125)) -
                  *(float *)(&g_object[0].lf_raw[0x71f0] + iVar15));
          fVar7 = (float)(((float)(((float)((&g_particle[0].unk_0018)[iVar6 * 0x3c] * fVar3 +
                   ((float)((&g_particle[0].unk_0018)[iVar4 * 0x3c] * fVar2 +
                   ((float)((&g_particle[0].unk_0018)[iVar5 * 0x3c] * fVar1 +
                   (float)((&g_particle[0].unk_0018)[iVar14 * 0x3c] * fVar17))))))) -
                  *(float *)((int)&g_object[0].rot[1].y + iVar16) * 0.125)) -
                  *(float *)(&g_object[0].lf_raw[0x71f4] + iVar15));
          fVar17 = (float)(((float)(((float)(fVar3 * (&g_particle[0].unk_001c)[iVar6 * 0x3c] +
                    ((float)(fVar2 * (&g_particle[0].unk_001c)[iVar4 * 0x3c] +
                    ((float)(fVar1 * (&g_particle[0].unk_001c)[iVar5 * 0x3c] +
                    (float)(fVar17 * (&g_particle[0].unk_001c)[iVar14 * 0x3c]))))))) -
                   *(float *)((int)&g_object[0].rot[1].z + iVar16) * 0.125)) -
                   *(float *)(&g_object[0].lf_raw[0x71f8] + iVar15));
          *(float *)(iVar16 + ((int)&g_object[0].unk_5f48) + local_50 * 0x98) =
               -((float)(fVar17 * *(float *)((int)&g_object[0].rot[1].z + iVar16) +
                (fVar7 * *(float *)((int)&g_object[0].rot[1].y + iVar16) +
                fVar8 * *(float *)((int)&g_object[0].rot[1].x + iVar16))));
          fVar8 = fVar8 * 0.25;
          fVar7 = fVar7 * 0.25;
          fVar17 = (float)(fVar17 * 0.25);
          fVar9 = (float)(*(float *)(&g_object[0].lf_raw[0x7290] + iVar15) * 0.5);
          fVar3 = (float)(*(float *)(&g_object[0].lf_raw[0x7294] + iVar15) * 0.5);
          fVar2 = (float)(*(float *)(&g_object[0].lf_raw[0x7298] + iVar15) * 0.5);
          fVar1 = (float)(*(float *)(&g_object[0].lf_raw[0x729c] + iVar15) * 0.5);
          fVar10 = (float)((float)(fVar2 * LF_BITS_F((&DAT_00583dd0)[iVar4 * 8]) +
                   ((float)((float)(((float)(*(float *)(&g_object[0].lf_raw[0x71fc] + iVar15) + 0.0)) * -0.5 +
                   fVar9 * LF_BITS_F((&DAT_00583dd0)[iVar14 * 8])) +
                   fVar3 * LF_BITS_F((&DAT_00583dd0)[iVar5 * 8])))) +
                   fVar1 * LF_BITS_F((&DAT_00583dd0)[iVar6 * 8]));
          fVar11 = (float)(LF_BITS_F((&DAT_00583dd4)[iVar6 * 8]) * fVar1 +
                   ((float)(LF_BITS_F((&DAT_00583dd4)[iVar4 * 8]) * fVar2 +
                   ((float)(LF_BITS_F((&DAT_00583dd4)[iVar5 * 8]) * fVar3 +
                   ((float)(LF_BITS_F((&DAT_00583dd4)[iVar14 * 8]) * fVar9 +
                   (float)(((float)(*(float *)(&g_object[0].lf_raw[0x7200] + iVar15) + 0.0)) * -0.5))))))));
          fVar2 = (float)(fVar1 * LF_BITS_F((&DAT_00583dd8)[iVar6 * 8]) +
                  ((float)(fVar2 * LF_BITS_F((&DAT_00583dd8)[iVar4 * 8]) +
                  ((float)(fVar3 * LF_BITS_F((&DAT_00583dd8)[iVar5 * 8]) +
                  ((float)(fVar9 * LF_BITS_F((&DAT_00583dd8)[iVar14 * 8]) +
                  (float)(((float)(*(float *)(&g_object[0].lf_raw[0x7204] + iVar15) + 0.0)) * -0.5))))))));
          fVar1 = (float)(fVar17 * *(float *)((int)&g_object[0].rot[1].z + iVar16) +
                  (fVar7 * *(float *)((int)&g_object[0].rot[1].y + iVar16) +
                  fVar8 * *(float *)((int)&g_object[0].rot[1].x + iVar16)));
          fVar3 = -fVar1;
          fVar1 = fVar1 * 0.125 * 0.375;
          local_74 = (float)((float)(fVar3 * *(float *)((int)&g_object[0].rot[1].x + iVar16) + fVar8) +
                     fVar1 * *(float *)((int)&g_object[0].rot[1].x + iVar16));
          local_70 = (float)(fVar1 * *(float *)((int)&g_object[0].rot[1].y + iVar16) +
                     ((float)(fVar3 * *(float *)((int)&g_object[0].rot[1].y + iVar16) + fVar7)));
          local_6c = (float)(fVar1 * *(float *)((int)&g_object[0].rot[1].z + iVar16) +
                     ((float)(fVar3 * *(float *)((int)&g_object[0].rot[1].z + iVar16) + fVar17)));
          fVar17 = (float)(fVar2 * *(float *)((int)&g_object[0].rot[1].z + iVar16) +
                   (fVar11 * *(float *)((int)&g_object[0].rot[1].y + iVar16) +
                   fVar10 * *(float *)((int)&g_object[0].rot[1].x + iVar16)));
          fVar1 = -fVar17;
          fVar17 = (float)(fVar17 * 0.375 * 0.5);
          local_68 = (float)((float)(fVar1 * *(float *)((int)&g_object[0].rot[1].x + iVar16) + fVar10) +
                     fVar17 * *(float *)((int)&g_object[0].rot[1].x + iVar16));
          local_64 = (float)(fVar17 * *(float *)((int)&g_object[0].rot[1].y + iVar16) +
                     ((float)(fVar1 * *(float *)((int)&g_object[0].rot[1].y + iVar16) + fVar11)));
          local_60 = (float)(fVar17 * *(float *)((int)&g_object[0].rot[1].z + iVar16) +
                     ((float)(fVar1 * *(float *)((int)&g_object[0].rot[1].z + iVar16) + fVar2)));
          local_10 = (float)(local_68 + local_74);
          local_c = (float)(local_64 + local_70);
          local_8 = (float)(local_60 + local_6c);
          local_4c = (float)((&g_particle[0].unk_0008)[iVar5 * 0x3c] *
                     *(float *)(&g_object[0].lf_raw[0x7294] + iVar15) +
                     (&g_particle[0].unk_0008)[iVar14 * 0x3c] *
                     *(float *)(&g_object[0].lf_raw[0x7290] + iVar15));
          local_58 = (float)((&g_particle[0].unk_0008)[iVar4 * 0x3c] *
                     *(float *)(&g_object[0].lf_raw[0x7298] + iVar15) +
                     (&g_particle[0].unk_0008)[iVar6 * 0x3c] *
                     *(float *)(&g_object[0].lf_raw[0x729c] + iVar15) + local_4c);
          local_5c = (float)(local_58 + *(float *)(&g_object[0].lf_raw[0x7274] + iVar15));
          local_58 = (float)(local_58 / local_5c);
          local_54 = (float)(*(float *)(&g_object[0].lf_raw[0x7274] + iVar15) / local_5c);
          *(float *)(&g_object[0].lf_raw[0x7208] + iVar15) =
               (float)(*(float *)(&g_object[0].lf_raw[0x7208] + iVar15) + local_58 * local_10);
          *(float *)(&g_object[0].lf_raw[0x720c] + iVar15) =
               (float)(local_c * local_58 + *(float *)(&g_object[0].lf_raw[0x720c] + iVar15));
          *(float *)(&g_object[0].lf_raw[0x7210] + iVar15) =
               (float)(*(float *)(&g_object[0].lf_raw[0x7210] + iVar15) + local_8 * local_58);
          local_48 = -local_54;
          fVar17 = local_48 * *(float *)(&g_object[0].lf_raw[0x7290] + iVar15);
          *(float *)&((&DAT_00583de0)[iVar14 * 8]) = (float)(local_10 * fVar17 + LF_BITS_F((&DAT_00583de0)[iVar14 * 8]));
          *(float *)&((&DAT_00583de4)[iVar14 * 8]) = (float)(local_c * fVar17 + LF_BITS_F((&DAT_00583de4)[iVar14 * 8]));
          *(float *)&((&DAT_00583de8)[iVar14 * 8]) = (float)(local_8 * fVar17 + LF_BITS_F((&DAT_00583de8)[iVar14 * 8]));
          fVar17 = (float)(local_48 * *(float *)(&g_object[0].lf_raw[0x7294] + iVar15));
          *(float *)&((&DAT_00583de0)[iVar5 * 8]) = (float)(local_10 * fVar17 + LF_BITS_F((&DAT_00583de0)[iVar5 * 8]));
          *(float *)&((&DAT_00583de4)[iVar5 * 8]) = (float)(local_c * fVar17 + LF_BITS_F((&DAT_00583de4)[iVar5 * 8]));
          *(float *)&((&DAT_00583de8)[iVar5 * 8]) = (float)(local_8 * fVar17 + LF_BITS_F((&DAT_00583de8)[iVar5 * 8]));
          local_78 = (float)(*(float *)(&g_object[0].lf_raw[0x7298] + iVar15) * local_48);
          *(float *)&((&DAT_00583de0)[iVar4 * 8]) = (float)(local_10 * local_78 + LF_BITS_F((&DAT_00583de0)[iVar4 * 8]));
          *(float *)&((&DAT_00583de4)[iVar4 * 8]) = (float)(local_c * local_78 + LF_BITS_F((&DAT_00583de4)[iVar4 * 8]));
          *(float *)&((&DAT_00583de8)[iVar4 * 8]) = (float)(local_8 * local_78 + LF_BITS_F((&DAT_00583de8)[iVar4 * 8]));
          local_48 = (float)(local_48 * *(float *)(&g_object[0].lf_raw[0x729c] + iVar15));
          local_50 = local_50 + 1;
          *(float *)&((&DAT_00583de0)[iVar6 * 8]) = (float)(LF_BITS_F((&DAT_00583de0)[iVar6 * 8]) + local_48 * local_10);
          *(float *)&((&DAT_00583de4)[iVar6 * 8]) = (float)(local_c * local_48 + LF_BITS_F((&DAT_00583de4)[iVar6 * 8]));
          *(float *)&((&DAT_00583de8)[iVar6 * 8]) = (float)(local_8 * local_48 + LF_BITS_F((&DAT_00583de8)[iVar6 * 8]));
        } while (local_50 < *(int *)((int)&g_object[0].unk_71ec + iVar16));
      }
    }
    local_44 = local_44 + 1;
    local_40 = local_40 + 0x208c;
    iVar16 = iVar16 + 0x8230;
    if (0x822fff < local_40) {
      return;
    }
  } while( true );
}
#undef g_object
#undef DAT_00460d18
#undef DAT_00603dd0
#undef DAT_00583dd0
#undef DAT_00583dd4
#undef DAT_00583dd8
#undef DAT_00583de0
#undef DAT_00583de4
#undef g_particle
#undef DAT_00583de8
#undef rotatevector
#undef getobjectrotation3
#undef __security_check_cookie
#undef local_78
#undef local_74
#undef local_70
#undef local_6c
#undef local_68
#undef local_64
#undef local_60
#undef local_5c
#undef local_58
#undef local_54
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
#undef local_10
#undef local_c
#undef local_8
#undef local_4

