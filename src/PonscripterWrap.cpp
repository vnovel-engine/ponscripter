/* PONScripter side of the shared total-fit planner.
 *
 * PONScripter already looks ahead: its LINE BREAKING ROUTINE (PonscripterLabel
 * ::parseLine) walks forward summing GlyphAdvance up to the next break token and
 * inserts a newline into current_text_buffer when the run does not fit. So the
 * two things the planner needs both exist, in a different shape:
 *
 *   candidates   the break tokens that routine already stops at
 *   run width    GlyphAdvance, summed -- and in float, so unlike ONScripter-EN
 *                there is no per-character fold to reproduce: summing the same
 *                floats the drawing loop adds gives exactly its answer
 *
 * What is missing is the *set* of candidates: the routine discovers them one at
 * a time while measuring, so there is no array for a planner to choose among.
 * wrapCandidates() builds that array from the same rules the routine uses, and
 * the planner then picks among them instead of taking the first that fits.
 *
 * The unit is pixels: Fontinfo::isNoRoomFor compares pos_x + margin against
 * area_x, and GlyphAdvance returns real advances, so run width and line room are
 * both pixels and are compared directly. */

#include "wrap.h"

#include "PonscripterLabel.h"
#include "Fontinfo.h"
#include "encoding.h"

/* A legal break before `offset` in the string buffer: the same condition
 * parseLine's routine stops at. Kept beside that routine's rules deliberately --
 * if one changes, the other must. */
static bool ponsIsBreakToken(const char *p)
{
    char c = *p;
    return (c == 0 || c == '\n' || c == '@' || c == '\\');
}

/* Collect the break candidates in [start,end) into `out`.
 * Returns how many were written. */
static int ponsCollectCandidates(PonscripterLabel *pons, const char *buf,
                                 int start, int end,
                                 WrapCandidate *out, int cap)
{
    int n = 0;
    for (int i = start + 1; i < end && n < cap; ) {
        int sz = pons->nextCharSizeAt(buf + i);
        if (sz <= 0) sz = 1;
        /* A break is legal before a token, and before a space that follows a
         * non-space (never inside a run of spaces). */
        if (ponsIsBreakToken(buf + i) ||
            ((buf[i] == ' ' || buf[i] == '\t') && buf[i-1] != ' ' && buf[i-1] != '\t'))
            out[n++].offset = i;
        i += sz;
    }
    return n;
}

/* --- the planner's three questions ---------------------------------------- */

float ponsWrapRunWidth(void *ctx, int from, int to)
{
    PonscripterLabel *pons = static_cast<PonscripterLabel *>(ctx);
    return pons->measureRunWidthPx(from, to);
}

float ponsWrapLineRoom(void *ctx)
{
    PonscripterLabel *pons = static_cast<PonscripterLabel *>(ctx);
    return pons->textAreaWidthPx();
}

float ponsWrapLineUsed(void *ctx)
{
    PonscripterLabel *pons = static_cast<PonscripterLabel *>(ctx);
    return pons->textCursorXPx();
}

int PonscripterLabel::nextCharSizeAt(const char *p)
{
    return file_encoding->NextCharSizeWithLigatures(p, &sentence_font);
}

float PonscripterLabel::measureRunWidthPx(int from, int to)
// Width of [from,to) in pixels, by the same advance the drawing loop adds.
//
// No fold to reproduce here, unlike ONScripter-EN: GlyphAdvance returns a float
// and Fontinfo accumulates pos_x as a float, so summing the same advances gives
// exactly what the cursor would move. That is the one place this engine is
// simpler than its sibling.
{
    const char *buf = script_h.getStrBuf(0);
    float w = 0.0f;
    for (int i = from; i < to; ) {
        int sz = nextCharSizeAt(buf + i);
        if (sz <= 0) sz = 1;
        wchar u = file_encoding->DecodeWithLigatures(buf + i, sentence_font);
        wchar next = file_encoding->DecodeWithLigatures(buf + i + sz, sentence_font);
        w += sentence_font.GlyphAdvance(u, next);
        i += sz;
    }
    return w;
}

float PonscripterLabel::textAreaWidthPx()
{
    return (float)sentence_font.area_x;
}

float PonscripterLabel::textCursorXPx()
{
    return sentence_font.GetXOffset();
}

int PonscripterLabel::planWrapSegment(int start, int end)
// Choose the breaks for [start,end): returns the number of breaks found.
//
// The choice is made by the shared planner (src/wrap.cpp) that ONScripter-EN
// also uses, so a page is broken by the same rules in both engines and one
// tuning pass covers them both.
{
    const char *buf = script_h.getStrBuf(0);
    if (!buf || end <= start)
        return 0;

    static const int MAXC = 512;
    WrapCandidate cand[MAXC];
    int nc = ponsCollectCandidates(this, buf, start, end, cand, MAXC);

    WrapEngine engine;
    engine.ctx = this;
    engine.run_width = ponsWrapRunWidth;
    engine.line_room = ponsWrapLineRoom;
    engine.line_used = ponsWrapLineUsed;

    if (!wrap_breaks) {
        wrap_breaks = new int[MAXC + 2];
        wrap_breaks_cap = MAXC + 2;
    }
    int n = wrap_total_fit(&engine, cand, nc, start, end,
                           wrap_breaks, wrap_breaks_cap);
    num_wrap_breaks = (n < 0) ? 0 : n;   // no fit: the old routine decides
    return num_wrap_breaks;
}
