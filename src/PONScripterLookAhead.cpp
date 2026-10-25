/*---------------------------------------------------------------------------
 * PONScripterLookAhead.cpp -- PONScripter's instantiation of the shared walk.
 *
 * THIS FILE LIVES IN THE PON TREE ON PURPOSE. `#include "ScriptHandler.h"`
 * resolves against the including file's own directory first, so a file under
 * onscripter-en/ can never see PONScripter's header whatever the -I order -- and
 * the two headers share an include guard, so one translation unit can only see
 * one of them. This small adapter is what lets the shared walk
 * (ONScripterLookAhead.h) serve both engines.
 */

#include "ScriptHandler.h"
#include "ONScripterLookAhead.h"

namespace {
/* The font whose ligature set the walk decodes with. It is a POINTER, set by the
 * caller just before the walk, because the walk is a free function and the font
 * belongs to the label (PonscripterLabel::sentence_font) -- the same font the
 * sentence being parsed will be drawn in. Null is safe: DecodeWithLigatures
 * dereferences it only to select a ligature subset, and the caller always sets
 * it. */
const Fontinfo *la_font = 0;

struct PonPolicy
{
    /* PONScripter's accessor differs from ONScripter's: pstring& getStrBuf(). */
    static const char *buf( ScriptHandler &sh ) { return sh.getStrBuf(); }
    /* The encoding's text marker (`^` UTF-8, '`' CP932), which the renderer
     * consumes and never draws. */
    static char text_marker( ScriptHandler &sh )
    {
        return file_encoding->TextMarker();
    }
    /* PONScripter decodes its script with the global `file_encoding`, chosen
     * from the script's own marker (UTF-8 or CP932), and its DecodeChar is what
     * the engine uses to rasterize. Using anything else (e.g. Shift_JIS
     * unconditionally) mangles every non-ASCII letter. DecodeChar also reports
     * how many bytes the character occupied, which the walk needs to advance.
     *
     * DecodeWithLigatures, not DecodeChar, because that is what the RENDERER
     * uses and the two differ on exactly the multi-byte sequences the reader
     * sees. PONScripter's ligature table maps `%.` to a thin space (U+2009),
     * `%_` to a no-break space and `%-` to a non-breaking hyphen
     * (encoding.cpp:150-152); using DecodeChar would publish the literal
     * three characters `%.` instead of the thin space the engine renders. The
     * ligature form is also the only one that can report a MULTI-character
     * sequence's byte length, which the walk needs to step over it at all.
     *
     * The Fontinfo argument is the ligature CONTEXT (a font switches a subset of
     * the table on), so the sentence font is passed by reference -- it is the
     * font the sentence is drawn in, and the page holds that sentence's text. */
    static unsigned short decode( const char *p, int &used )
    {
        return (unsigned short)file_encoding->DecodeWithLigatures(
            p, *la_font, used );
    }
};
} /* namespace */

/* Called from the run loop, before a line is dispatched.
 *
 * `font` is the label's sentence font, which the walk needs for the ligature
 * table (see PonPolicy::decode). */
int PonscripterLabelLookAhead( ScriptHandler &sh, const Fontinfo &font )
{
    la_font = &font;
    const int n = la_walk<ScriptHandler, PonPolicy>( sh );
    la_font = 0;
    return n;
}
