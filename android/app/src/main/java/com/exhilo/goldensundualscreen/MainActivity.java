package com.exhilo.goldensundualscreen;

import android.app.Presentation;
import android.content.Context;
import android.content.Intent;
import android.hardware.display.DisplayManager;
import android.net.Uri;
import android.os.Bundle;
import android.view.Display;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

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
