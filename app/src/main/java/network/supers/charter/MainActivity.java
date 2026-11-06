package network.supers.charter;

import android.app.Activity;
import android.content.ClipData;
import android.content.Intent;
import android.content.res.ColorStateList;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.provider.OpenableColumns;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.SeekBar;
import android.widget.TextView;
import android.widget.Toast;
import android.widget.VideoView;

import org.json.JSONArray;
import org.json.JSONException;

import java.util.ArrayList;

public final class MainActivity extends Activity {
    private static final int PICK_MEDIA = 100;
    private static final String PREFS = "charter_library";
    private final int[] colors = CharterGapi.palette();
    private final ArrayList<Uri> media = new ArrayList<>();
    private final ArrayList<String> titles = new ArrayList<>();
    private final Handler clock = new Handler(Looper.getMainLooper());
    private ArrayAdapter<String> adapter;
    private VideoView player;
    private SeekBar seek;
    private Button playPause;
    private TextView nowPlaying;
    private int currentIndex = -1;
    private boolean prepared;

    private final Runnable progressTick = new Runnable() {
        @Override public void run() {
            if (prepared && player.isPlaying()) {
                seek.setMax(Math.max(1, player.getDuration()));
                seek.setProgress(player.getCurrentPosition());
            }
            clock.postDelayed(this, 500);
        }
    };

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        int[] metrics = CharterGapi.metrics(getResources().getConfiguration().screenWidthDp);
        getWindow().setStatusBarColor(colors[1]);
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setPadding(dp(metrics[0]), dp(metrics[0]), dp(metrics[0]), dp(metrics[0]));
        root.setBackgroundColor(colors[0]);

        TextView heading = label("Charter Media Player", 24);
        root.addView(heading);
        Button importButton = new Button(this);
        importButton.setText("Import audio or video");
        importButton.setBackgroundTintList(ColorStateList.valueOf(colors[2]));
        importButton.setOnClickListener(view -> openPicker());
        root.addView(importButton);

        player = new VideoView(this);
        player.setBackgroundColor(colors[1]);
        root.addView(player, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, dp(metrics[2])));
        nowPlaying = label("Nothing playing", 16);
        root.addView(nowPlaying);

        ListView library = new ListView(this);
        adapter = new ArrayAdapter<>(this, android.R.layout.simple_list_item_1, titles);
        library.setBackgroundColor(colors[1]);
        library.setAdapter(adapter);
        library.setOnItemClickListener((parent, view, position, id) -> play(position));
        root.addView(library, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, 0, 1));

        LinearLayout controls = new LinearLayout(this);
        Button previous = new Button(this);
        previous.setText("Previous");
        previous.setOnClickListener(view -> skip(-1));
        controls.addView(previous, new LinearLayout.LayoutParams(0, dp(48), 1));
        playPause = new Button(this);
        playPause.setText("Play");
        playPause.setOnClickListener(view -> togglePlayback());
        controls.addView(playPause, new LinearLayout.LayoutParams(0, dp(48), 1));
        Button next = new Button(this);
        next.setText("Next");
        next.setOnClickListener(view -> skip(1));
        controls.addView(next, new LinearLayout.LayoutParams(0, dp(48), 1));
        root.addView(controls);
        seek = new SeekBar(this);
        seek.setProgressTintList(ColorStateList.valueOf(colors[2]));
        seek.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override public void onProgressChanged(SeekBar bar, int value, boolean fromUser) { }
            @Override public void onStartTrackingTouch(SeekBar bar) { }
            @Override public void onStopTrackingTouch(SeekBar bar) {
                if (prepared) player.seekTo(bar.getProgress());
            }
        });
        root.addView(seek);
        setContentView(root);

        player.setOnPreparedListener(value -> {
            prepared = true;
            seek.setMax(Math.max(1, player.getDuration()));
            player.start();
            playPause.setText("Pause");
        });
        player.setOnCompletionListener(value -> skip(1));
        player.setOnErrorListener((value, what, extra) -> {
            prepared = false;
            playPause.setText("Play");
            Toast.makeText(this, "This file could not be played", Toast.LENGTH_LONG).show();
            return true;
        });
        loadLibrary();
        clock.post(progressTick);
    }

    private TextView label(String text, int size) {
        TextView view = new TextView(this);
        view.setText(text);
        view.setTextSize(size);
        view.setTextColor(colors[3]);
        view.setPadding(0, dp(8), 0, dp(8));
        return view;
    }

    private int dp(int value) {
        return Math.round(value * getResources().getDisplayMetrics().density);
    }

    private void openPicker() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("*/*");
        intent.putExtra(Intent.EXTRA_MIME_TYPES, new String[] { "audio/*", "video/*" });
        intent.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true);
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION |
                Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
        startActivityForResult(intent, PICK_MEDIA);
    }

    @Override protected void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request, result, data);
        if (request != PICK_MEDIA || result != RESULT_OK || data == null) return;
        ClipData clips = data.getClipData();
        if (clips != null) {
            for (int i = 0; i < clips.getItemCount(); i++) addMedia(clips.getItemAt(i).getUri());
        } else if (data.getData() != null) {
            addMedia(data.getData());
        }
        saveLibrary();
    }

    private void addMedia(Uri uri) {
        if (media.contains(uri)) return;
        try {
            getContentResolver().takePersistableUriPermission(uri,
                    Intent.FLAG_GRANT_READ_URI_PERMISSION);
        } catch (SecurityException ignored) {
            // Some providers only grant access for this session.
        }
        media.add(uri);
        titles.add(titleOf(uri));
        adapter.notifyDataSetChanged();
    }

    private String titleOf(Uri uri) {
        try (Cursor cursor = getContentResolver().query(uri,
                new String[] { OpenableColumns.DISPLAY_NAME }, null, null, null)) {
            if (cursor != null && cursor.moveToFirst()) {
                String name = cursor.getString(0);
                if (name != null && !name.isEmpty()) return name;
            }
        } catch (Exception ignored) { }
        return uri.getLastPathSegment() == null ? "Unknown media" : uri.getLastPathSegment();
    }

    private void loadLibrary() {
        try {
            JSONArray saved = new JSONArray(getPreferences(MODE_PRIVATE).getString(PREFS, "[]"));
            for (int i = 0; i < saved.length(); i++) addMedia(Uri.parse(saved.getString(i)));
        } catch (JSONException ignored) { }
    }

    private void saveLibrary() {
        JSONArray saved = new JSONArray();
        for (Uri uri : media) saved.put(uri.toString());
        getPreferences(MODE_PRIVATE).edit().putString(PREFS, saved.toString()).apply();
    }

    private void play(int index) {
        if (index < 0 || index >= media.size()) return;
        currentIndex = index;
        prepared = false;
        seek.setProgress(0);
        nowPlaying.setText(titles.get(index));
        playPause.setText("Loading…");
        player.setVideoURI(media.get(index));
        player.requestFocus();
    }

    private void togglePlayback() {
        if (currentIndex < 0) {
            if (!media.isEmpty()) play(0);
        } else if (prepared && player.isPlaying()) {
            player.pause();
            playPause.setText("Play");
        } else if (prepared) {
            player.start();
            playPause.setText("Pause");
        }
    }

    private void skip(int offset) {
        if (media.isEmpty()) return;
        int next = currentIndex < 0 ? 0 : (currentIndex + offset + media.size()) % media.size();
        play(next);
    }

    @Override protected void onDestroy() {
        clock.removeCallbacks(progressTick);
        player.stopPlayback();
        super.onDestroy();
    }
}
