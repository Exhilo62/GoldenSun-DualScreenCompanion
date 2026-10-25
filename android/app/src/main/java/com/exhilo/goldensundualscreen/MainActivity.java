package com.exhilo.goldensundualscreen;

import android.app.Presentation;
import android.content.Context;
import android.content.Intent;
import android.hardware.display.DisplayManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.Display;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;

import org.libsdl.app.SDLActivity;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;

/**
 * The whole app is SDL (see desktop/platform_sdl.c). This class names the native libraries and, on a device with two displays (Retroid Pocket Duo and
 * similar), puts the GBA picture on the other display: the screen this app was started on stays the touch screen with the companion UI, the other one
 * shows the game. On a single-display phone nothing here does anything.
 */
public class MainActivity extends SDLActivity {
    private static native void nativeSetTopSurface(Surface surface);
    private static native void nativeSurfaceTouch(int action, float x, float y);

    private DisplayManager displays;
    private Presentation topScreen;

    private final DisplayManager.DisplayListener listener = new DisplayManager.DisplayListener() {
        @Override public void onDisplayAdded(int id) { refreshTopScreen(); }
        @Override public void onDisplayRemoved(int id) { refreshTopScreen(); }
        @Override public void onDisplayChanged(int id) { }
    };

    /**
     * Hides the status bar and the navigation bar / gesture pill and keeps them hidden: Android shows them again after a touch on the other display, a dialog or
     * the file picker, so this runs on every focus change and whenever the system reports that the bars came back. A swipe from the edge still shows them for a moment.
     */
    static void hideSystemBars(final Window w) {
        if (w == null) return;
        final View decor = w.getDecorView();
        if (Build.VERSION.SDK_INT >= 28) {
            WindowManager.LayoutParams lp = w.getAttributes();
            lp.layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
            w.setAttributes(lp);
        }
        if (Build.VERSION.SDK_INT >= 30) {
            WindowInsetsController c = w.getInsetsController();
            if (c != null) {
                c.hide(WindowInsets.Type.systemBars());
                c.setSystemBarsBehavior(WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            }
        } else {
            decor.setSystemUiVisibility(View.SYSTEM_UI_FLAG_LAYOUT_STABLE | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
        }
        decor.setOnSystemUiVisibilityChangeListener(new View.OnSystemUiVisibilityChangeListener() {
            @Override public void onSystemUiVisibilityChange(int visibility) {
                if ((visibility & View.SYSTEM_UI_FLAG_HIDE_NAVIGATION) == 0)
                    new Handler(Looper.getMainLooper()).postDelayed(new Runnable() { @Override public void run() { hideSystemBars(w); } }, 1500);
            }
        });
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) hideSystemBars(getWindow());
    }

    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL2", "main" };
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        displays = (DisplayManager) getSystemService(Context.DISPLAY_SERVICE);
        displays.registerDisplayListener(listener, null);
        hideSystemBars(getWindow());
    }

    @Override
    protected void onResume() {
        super.onResume();
        refreshTopScreen();
        hideSystemBars(getWindow());
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
                // This window must never take input focus: Android sends controller and key events to the focused window of the most recently touched
                // display, so touching the other screen would otherwise pull the buttons away from the game. Touches still arrive.
                getWindow().addFlags(WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE | WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL);
                SurfaceView view = new SurfaceView(getContext());
                view.getHolder().addCallback(new SurfaceHolder.Callback() {
                    @Override public void surfaceCreated(SurfaceHolder h) { nativeSetTopSurface(h.getSurface()); }
                    @Override public void surfaceChanged(SurfaceHolder h, int f, int w, int hh) { nativeSetTopSurface(h.getSurface()); }
                    @Override public void surfaceDestroyed(SurfaceHolder h) { nativeSetTopSurface(null); }
                });
                // When the screens are swapped the companion UI is drawn here, so a finger on this display is the touch screen.
                view.setOnTouchListener(new View.OnTouchListener() {
                    @Override
                    public boolean onTouch(View v, MotionEvent e) {
                        int a = e.getActionMasked();
                        if (a == MotionEvent.ACTION_DOWN || a == MotionEvent.ACTION_MOVE) nativeSurfaceTouch(0, e.getX() / v.getWidth(), e.getY() / v.getHeight());
                        else if (a == MotionEvent.ACTION_UP || a == MotionEvent.ACTION_CANCEL) nativeSurfaceTouch(1, 0, 0);
                        return true;
                    }
                });
                hideSystemBars(getWindow());   // no status / navigation bar over the picture (or the UI) on this display either
                view.setFocusable(false);
                view.setFocusableInTouchMode(false);
                setContentView(view);
            }
        };
        try {
            topScreen.show();
            hideSystemBars(topScreen.getWindow());   // now attached: the insets controller exists
        } catch (RuntimeException e) {   // the display vanished between the lookup and show()
            topScreen = null;
        }
    }

    // ---- ROM picker ------------------------------------------------------------------------------------------------------
    // Called from the native code: opens the system file picker, validates the chosen file's header (Golden Sun = AGS*, The Lost Age = AGF*) and
    // copies it into the app's data folder as golden_sun.gba / lost_age.gba. The ROM never leaves the device.
    private static final int REQ_ROM = 4711;
    private static volatile int pickState;   // 0 idle or cancelled, 1 working, 2 added, 3 not a Golden Sun ROM, 4 could not read or write
    private static volatile String pickName = "";

    public static void pickRom() {
        final MainActivity a = (MainActivity) SDLActivity.getContext();
        if (a == null) { pickState = 4; return; }
        pickState = 1;
        a.runOnUiThread(new Runnable() {
            @Override public void run() {
                Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
                i.addCategory(Intent.CATEGORY_OPENABLE);
                i.setType("*/*");
                try {
                    a.startActivityForResult(i, REQ_ROM);
                } catch (RuntimeException e) {
                    pickState = 4;
                }
            }
        });
    }

    public static int pickState() { return pickState; }
    public static String pickName() { return pickName; }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, final Intent data) {
        if (requestCode != REQ_ROM) { super.onActivityResult(requestCode, resultCode, data); return; }
        if (resultCode != RESULT_OK || data == null || data.getData() == null) { pickState = 0; return; }
        final Uri uri = data.getData();
        new Thread(new Runnable() {
            @Override public void run() { copyRom(uri); }
        }).start();
    }

    private void copyRom(Uri uri) {
        File dir = getExternalFilesDir(null);
        if (dir == null) dir = getFilesDir();
        File tmp = new File(dir, "incoming.tmp");
        try (InputStream in = getContentResolver().openInputStream(uri); FileOutputStream out = new FileOutputStream(tmp)) {
            if (in == null) { pickState = 4; return; }
            byte[] buf = new byte[1 << 16];
            long total = 0;
            int n = in.read(buf);
            String code = n >= 0xB0 ? new String(buf, 0xAC, 3, "US-ASCII") : "";
            String target = code.equals("AGS") ? "golden_sun.gba" : code.equals("AGF") ? "lost_age.gba" : null;
            if (target == null) { pickState = 3; tmp.delete(); return; }
            pickName = target;
            while (n > 0) {
                out.write(buf, 0, n);
                total += n;
                if (total > 33554432L) { pickState = 3; tmp.delete(); return; }
                n = in.read(buf);
            }
            out.flush();
            out.close();
            File dest = new File(dir, target);
            if (dest.exists()) dest.delete();
            pickState = tmp.renameTo(dest) ? 2 : 4;
        } catch (Exception e) {
            tmp.delete();
            pickState = 4;
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
