#include "aampcli_kmp.h"
#include "main_aamp.h"
#include <gst/gst.h>
#include <queue>

static PlayerInstanceAAMP *g_kmp_player;
static GMainLoop *main_loop;

static gboolean tuneFunc( gpointer arg )
{	char *url = (char *)arg;
	if( url )
	{
		printf( "tuneFunc(\"%s\")\n", url );
		if( !g_kmp_player )
		{ // lazily allocate player instance
			g_kmp_player = new PlayerInstanceAAMP();
		}
		else {
			g_kmp_player->Stop();
		}
		g_kmp_player->Tune(url);
		free( url );
	}
	return G_SOURCE_REMOVE;
}

static int main_func( gpointer user_data )
{
	GMainContext *context = NULL;
	gboolean is_running = FALSE;
	main_loop = g_main_loop_new(context,is_running);
	g_main_loop_run(main_loop);
	g_main_loop_unref(main_loop);
	return 0;
}

extern "C" void kmp_init( void )
{
#if defined(__APPLE__) && defined(__aarch64__)
// this workaround needed/working only on new 64 bit mac
		gst_macos_main_simple(main_func, NULL );
#else
		main_func(NULL);
#endif
}

extern "C" void kmp_tune( const char *url )
{
	gpointer data = strdup(url);
	int event_source = g_idle_add( tuneFunc, data );
	(void)event_source;
}
