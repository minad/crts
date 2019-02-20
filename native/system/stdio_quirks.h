/*
 * Export some stdio functions under a different
 * name to ensure binary compatibility.
 * stdout etc are available on Windows as _iob,
 * ftello etc are called ftello64, or _ftelli64.
 */

extern CHI_API FILE *chi_stdout, *chi_stderr, *chi_stdin;
CHI_API int chi_fseeko(FILE*, int64_t, int);
CHI_API int64_t chi_ftello(FILE*);

FILE *chi_stdout, *chi_stderr, *chi_stdin;

int chi_fseeko(FILE* fp, int64_t off, int whence) {
    return fseeko(fp, (off_t)off, whence);
}

int64_t chi_ftello(FILE* fp) {
    return (int64_t)ftello(fp);
}

CHI_CONSTRUCTOR {
    chi_stdout = stdout;
    chi_stderr = stderr;
    chi_stdin  = stdin;
}
