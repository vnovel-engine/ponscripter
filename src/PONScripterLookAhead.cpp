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
     * the engine uses to rasterize. Using anything else -- the walk once used
     * Shift_JIS unconditionally -- mangles every non-ASCII letter: Le sanglot's
     * French arrived as `derni?re` for `dernière`. DecodeChar also reports how
     * many bytes the character occupied, which the walk needs to advance. */
    static unsigned short decode( const char *p, int &used )
    {
        return (unsigned short)file_encoding->DecodeChar( p, used );
    }
};
} /* namespace */

/* Called from the run loop, before a line is dispatched. */
int PonscripterLabelLookAhead( ScriptHandler &sh )
{
    return la_walk<ScriptHandler, PonPolicy>( sh );
}
