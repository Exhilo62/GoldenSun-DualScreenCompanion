package com.exhilo.goldensundualscreen;

import android.app.Presentation;
import android.content.Context;
import android.hardware.display.DisplayManager;
import android.os.Bundle;
import android.view.Display;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import org.libsdl.app.SDLActivity;

/**
 * The whole app is SDL (see desktop/platform_sdl.c). This class names the native libraries and, on a device with two displays (Retroid Pocket Duo and
 * similar), puts the GBA picture on the other display: the screen this app was started on stays the touch screen with the companion UI, the other one
 * shows the game. On a single-display phone nothing here does anything.
 */
public class MainActivity extends SDLActivity {
    private static native void nativeSetTopSurface(Surface surface);

    private DisplayManager displays;
    private Presentation topScreen;

    private final DisplayManager.DisplayListener listener = new DisplayManager.DisplayListener() {
        @Override public void onDisplayAdded(int id) { refreshTopScreen(); }
        @Override public void onDisplayRemoved(int id) { refreshTopScreen(); }
        @Override public void onDisplayChanged(int id) { }
    };

    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL2", "main" };
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        displays = (DisplayManager) getSystemService(Context.DISPLAY_SERVICE);
        displays.registerDisplayListener(listener, null);
    }

    @Override
    protected void onResume() {
        super.onResume();
        refreshTopScreen();
    }

    @Override
    protected void onPause() {
        hideTopScreen();
        super.onPause();
    }

    @Override
    protected void onDestroy() {
        if (displays != null) displays.unregisterDisplayListener(listener);
        hideTopScreen();
        super.onDestroy();
    }

    /** The display this activity is on. */
    private Display myDisplay() {
        Display d = getWindow().getDecorView().getDisplay();
        return d != null ? d : getWindowManager().getDefaultDisplay();
    }

    private void refreshTopScreen() {
        hideTopScreen();
        Display mine = myDisplay();
        Display other = null;
        for (Display d : displays.getDisplays(DisplayManager.DISPLAY_CATEGORY_PRESENTATION)) {
            if (mine == null || d.getDisplayId() != mine.getDisplayId()) { other = d; break; }
        }
        if (other == null) {   // some handhelds do not tag the second display for presentations
            for (Display d : displays.getDisplays()) {
                if (mine != null && d.getDisplayId() != mine.getDisplayId() && d.getState() != Display.STATE_OFF) { other = d; break; }
            }
        }
        if (other == null) return;
        topScreen = new Presentation(this, other) {
            @Override
            protected void onCreate(Bundle b) {
                super.onCreate(b);
                SurfaceView view = new SurfaceView(getContext());
                view.getHolder().addCallback(new SurfaceHolder.Callback() {
                    @Override public void surfaceCreated(SurfaceHolder h) { nativeSetTopSurface(h.getSurface()); }
                    @Override public void surfaceChanged(SurfaceHolder h, int f, int w, int hh) { nativeSetTopSurface(h.getSurface()); }
                    @Override public void surfaceDestroyed(SurfaceHolder h) { nativeSetTopSurface(null); }
                });
                setContentView(view);
            }
        };
        try {
            topScreen.show();
        } catch (RuntimeException e) {   // the display vanished between the lookup and show()
            topScreen = null;
        }
    }

    private void hideTopScreen() {
        if (topScreen != null) {
            try { topScreen.dismiss(); } catch (RuntimeException ignored) { }
            topScreen = null;
        }
        nativeSetTopSurface(null);
    }
}
