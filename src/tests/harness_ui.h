/* Console output of the harness: header, run information, one line per function and a
 * summary. Colors only when stdout is a console. A redirected run writes plain text, and its
 * last line stays "N functions tested: ..." for scripts. */
#ifndef HARNESS_UI_H
#define HARNESS_UI_H

void ui_init(void);
void ui_banner(void);
void ui_info(const char *label, const char *fmt, ...);
void ui_warning(const char *fmt, ...);
void ui_function(int index, int total, const char *name, int same, int diff, int crash, int skip, int known,
                 const char *note);
void ui_summary(int tested, int same, int diff, int known, int crash, int skip, double seconds);

#endif /* HARNESS_UI_H */
