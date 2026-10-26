/* Connect the game loop to the hook profiler. */
void sv_hooks_lifted_profile(void);

void sv_profile_report(void)
{
    sv_hooks_lifted_profile();
}
