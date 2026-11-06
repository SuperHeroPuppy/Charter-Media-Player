package network.supers.charter;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.ClipData;
import android.content.ComponentName;
import android.content.Intent;
import android.content.UriPermission;
import android.content.res.ColorStateList;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.ParcelFileDescriptor;
import android.provider.OpenableColumns;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.SeekBar;
import android.widget.TextView;
import android.widget.Toast;

import androidx.media3.common.MediaItem;
import androidx.media3.common.MediaMetadata;
import androidx.media3.common.PlaybackException;
import androidx.media3.common.Player;
import androidx.media3.session.MediaController;
import androidx.media3.session.SessionToken;
import androidx.media3.ui.PlayerView;

import com.google.common.util.concurrent.ListenableFuture;

import org.json.JSONArray;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.Locale;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public final class MainActivity extends Activity {
    private static final int PICK_MEDIA = 100;
    private static final String PREFS = "charter_library";

    private static final class Entry {
        final Uri uri;
        final String title;
        final boolean video;
        Entry(Uri uri, String title, boolean video) {
            this.uri = uri;
            this.title = title;
            this.video = video;
        }
    }

    private final int[] colors = CharterGapi.palette();
    private final ArrayList<Entry> media = new ArrayList<>();
    private final ArrayList<Integer> visibleIndices = new ArrayList<>();
    private final ArrayList<String> visibleTitles = new ArrayList<>();
    private final ExecutorService storage = Executors.newSingleThreadExecutor();
    private final Handler clock = new Handler(Looper.getMainLooper());
    private ArrayAdapter<String> adapter;
    private PlayerView videoView;
    private SeekBar seek;
    private Button playPause;
    private Button queueButton;
    private Button clearSearch;
    private EditText search;
    private TextView nowPlaying;
    private TextView time;
    private ListenableFuture<MediaController> controllerFuture;
    private MediaController controller;
    private Intent pendingOpen;
    private int pendingIndex = -1;
    private boolean queueMode;
    private boolean destroyed;

    private final Player.Listener playbackListener = new Player.Listener() {
        @Override public void onEvents(Player player, Player.Events events) {
            updatePlaybackUi();
            if (queueMode) refreshList();
        }
        @Override public void onPlayerError(PlaybackException error) {
            Toast.makeText(MainActivity.this, "This file could not be played", Toast.LENGTH_LONG).show();
        }
    };

    private final Runnable progressTick = new Runnable() {
        @Override public void run() {
            updateProgress();
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
        root.addView(label("Charter Media Player", 24));

        Button importButton = new Button(this);
        importButton.setText("Import audio or video");
        importButton.setBackgroundTintList(ColorStateList.valueOf(colors[2]));
        importButton.setOnClickListener(view -> openPicker());
        root.addView(importButton);

        videoView = new PlayerView(this);
        videoView.setUseController(false);
        videoView.setVisibility(View.GONE);
        root.addView(videoView, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, dp(metrics[2])));
        nowPlaying = label("Nothing playing", 16);
        root.addView(nowPlaying);
        time = label("0:00 / 0:00", 13);
        root.addView(time);

        LinearLayout listTools = new LinearLayout(this);
        search = new EditText(this);
        search.setSingleLine(true);
        search.setTextColor(colors[3]);
        search.setHintTextColor(0xff999999);
        search.setHint("Search library");
        search.addTextChangedListener(new TextWatcher() {
            @Override public void beforeTextChanged(CharSequence value, int start, int count, int after) { }
            @Override public void onTextChanged(CharSequence value, int start, int before, int count) {
                clearSearch.setVisibility(value.length() == 0 || queueMode ? View.GONE : View.VISIBLE);
                if (!queueMode) refreshList();
            }
            @Override public void afterTextChanged(Editable value) { }
        });
        listTools.addView(search, new LinearLayout.LayoutParams(0, dp(48), 1));
        clearSearch = new Button(this);
        clearSearch.setText("×");
        clearSearch.setContentDescription("Clear search");
        clearSearch.setVisibility(View.GONE);
        clearSearch.setOnClickListener(view -> search.setText("") );
        listTools.addView(clearSearch, new LinearLayout.LayoutParams(dp(44), dp(48)));
        queueButton = new Button(this);
        queueButton.setText("Queue");
        queueButton.setOnClickListener(view -> {
            queueMode = !queueMode;
            queueButton.setText(queueMode ? "Library" : "Queue");
            search.setVisibility(queueMode ? View.GONE : View.VISIBLE);
            clearSearch.setVisibility(queueMode || search.length() == 0 ? View.GONE : View.VISIBLE);
            refreshList();
        });
        listTools.addView(queueButton);
        root.addView(listTools);

        ListView library = new ListView(this);
        adapter = new ArrayAdapter<>(this, android.R.layout.simple_list_item_1, visibleTitles);
        library.setBackgroundColor(colors[1]);
        library.setAdapter(adapter);
        library.setOnItemClickListener((parent, view, position, id) -> {
            if (queueMode) {
                if (controller != null && position < controller.getMediaItemCount()) {
                    controller.seekTo(position, 0);
                    controller.play();
                }
            } else if (position < visibleIndices.size()) play(visibleIndices.get(position));
        });
        library.setOnItemLongClickListener((parent, view, position, id) -> {
            if (queueMode && controller != null && position < controller.getMediaItemCount()) {
                showQueueActions(position);
                return true;
            } else if (!queueMode && position < visibleIndices.size()) {
                showLibraryActions(visibleIndices.get(position));
                return true;
            }
            return false;
        });
        root.addView(library, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, 0, 1));

        LinearLayout controls = new LinearLayout(this);
        Button previous = new Button(this);
        previous.setText("Previous");
        previous.setOnClickListener(view -> previous());
        controls.addView(previous, new LinearLayout.LayoutParams(0, dp(48), 1));
        playPause = new Button(this);
        playPause.setText("Play");
        playPause.setOnClickListener(view -> togglePlayback());
        controls.addView(playPause, new LinearLayout.LayoutParams(0, dp(48), 1));
        Button next = new Button(this);
        next.setText("Next");
        next.setOnClickListener(view -> next());
        controls.addView(next, new LinearLayout.LayoutParams(0, dp(48), 1));
        root.addView(controls);

        seek = new SeekBar(this);
        seek.setProgressTintList(ColorStateList.valueOf(colors[2]));
        seek.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override public void onProgressChanged(SeekBar bar, int value, boolean fromUser) { }
            @Override public void onStartTrackingTouch(SeekBar bar) { }
            @Override public void onStopTrackingTouch(SeekBar bar) {
                if (controller != null && controller.getMediaItemCount() > 0)
                    controller.seekTo(bar.getProgress());
            }
        });
        root.addView(seek);
        setContentView(root);

        loadLibrary();
        if (Intent.ACTION_VIEW.equals(getIntent().getAction())) pendingOpen = getIntent();
    }

    @Override protected void onStart() {
        super.onStart();
        SessionToken token = new SessionToken(this, new ComponentName(this, PlaybackService.class));
        ListenableFuture<MediaController> requested =
                new MediaController.Builder(this, token).buildAsync();
        controllerFuture = requested;
        requested.addListener(() -> {
            if (destroyed || controllerFuture != requested) return;
            try {
                controller = requested.get();
                controller.addListener(playbackListener);
                videoView.setPlayer(controller);
                updatePlaybackUi();
                if (pendingOpen != null) {
                    Intent intent = pendingOpen;
                    pendingOpen = null;
                    openExternal(intent);
                } else if (pendingIndex >= 0) {
                    int index = pendingIndex;
                    pendingIndex = -1;
                    play(index);
                }
            } catch (Exception error) {
                Toast.makeText(this, "Playback service unavailable", Toast.LENGTH_LONG).show();
            }
        }, getMainExecutor());
        clock.post(progressTick);
    }

    @Override protected void onStop() {
        clock.removeCallbacks(progressTick);
        if (controller != null) {
            videoView.setPlayer(null);
            controller.removeListener(playbackListener);
            controller = null;
        }
        if (controllerFuture != null) {
            MediaController.releaseFuture(controllerFuture);
            controllerFuture = null;
        }
        super.onStop();
    }

    @Override protected void onDestroy() {
        destroyed = true;
        storage.shutdownNow();
        super.onDestroy();
    }

    @Override protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        if (!Intent.ACTION_VIEW.equals(intent.getAction())) return;
        if (controller == null) pendingOpen = intent;
        else openExternal(intent);
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
        ArrayList<Uri> selected = new ArrayList<>();
        ClipData clips = data.getClipData();
        if (clips != null) {
            for (int i = 0; i < clips.getItemCount(); i++) selected.add(clips.getItemAt(i).getUri());
        } else if (data.getData() != null) selected.add(data.getData());
        storage.execute(() -> {
            ArrayList<Entry> imported = new ArrayList<>();
            int failed = 0;
            for (Uri uri : selected) {
                try {
                    getContentResolver().takePersistableUriPermission(uri,
                            Intent.FLAG_GRANT_READ_URI_PERMISSION);
                } catch (SecurityException ignored) { }
                if (!hasPersistedAccess(uri) || !canRead(uri)) {
                    failed++;
                    continue;
                }
                String title = titleOf(uri);
                imported.add(new Entry(uri, title, isVideo(uri, title, null)));
            }
            int failures = failed;
            runOnUiThread(() -> {
                if (destroyed) return;
                for (Entry entry : imported) addMedia(entry);
                saveLibrary();
                refreshList();
                if (failures > 0) Toast.makeText(this,
                        failures + " file(s) could not be kept in the library", Toast.LENGTH_LONG).show();
            });
        });
    }

    private boolean hasPersistedAccess(Uri uri) {
        for (UriPermission permission : getContentResolver().getPersistedUriPermissions())
            if (permission.isReadPermission() && permission.getUri().equals(uri)) return true;
        return false;
    }

    private boolean canRead(Uri uri) {
        try (ParcelFileDescriptor file = getContentResolver().openFileDescriptor(uri, "r")) {
            return file != null;
        } catch (Exception error) { return false; }
    }

    private void addMedia(Entry entry) {
        for (Entry existing : media) if (existing.uri.equals(entry.uri)) return;
        media.add(entry);
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

    private boolean isVideo(Uri uri, String title, String explicitMime) {
        String mime = explicitMime;
        if (mime == null) mime = getContentResolver().getType(uri);
        return (mime != null && mime.startsWith("video/")) ||
                title.toLowerCase(Locale.ROOT).matches(".*\\.(mp4|m4v|mov|mkv|webm|avi)$");
    }

    private void loadLibrary() {
        String json = getPreferences(MODE_PRIVATE).getString(PREFS, "[]");
        storage.execute(() -> {
            ArrayList<Entry> saved = new ArrayList<>();
            boolean changed = false;
            try {
                JSONArray records = new JSONArray(json);
                for (int i = 0; i < records.length(); i++) {
                    Object record = records.get(i);
                    Uri uri;
                    String title;
                    boolean video;
                    if (record instanceof JSONObject) {
                        JSONObject object = (JSONObject) record;
                        uri = Uri.parse(object.getString("uri"));
                        title = object.optString("title", "Unknown media");
                        video = object.optBoolean("video", false);
                    } else {
                        uri = Uri.parse((String) record);
                        title = titleOf(uri);
                        video = isVideo(uri, title, null);
                        changed = true;
                    }
                    if (hasPersistedAccess(uri) && canRead(uri)) saved.add(new Entry(uri, title, video));
                    else changed = true;
                }
            } catch (Exception error) { changed = true; }
            boolean rewrite = changed;
            runOnUiThread(() -> {
                if (destroyed) return;
                for (Entry entry : saved) addMedia(entry);
                refreshList();
                if (rewrite) saveLibrary();
            });
        });
    }

    private void saveLibrary() {
        JSONArray saved = new JSONArray();
        for (Entry entry : media) {
            JSONObject object = new JSONObject();
            try {
                object.put("uri", entry.uri.toString());
                object.put("title", entry.title);
                object.put("video", entry.video);
                saved.put(object);
            } catch (Exception ignored) { }
        }
        getPreferences(MODE_PRIVATE).edit().putString(PREFS, saved.toString()).apply();
    }

    private MediaItem mediaItem(Entry entry) {
        return new MediaItem.Builder().setMediaId(entry.uri.toString()).setUri(entry.uri)
                .setMediaMetadata(new MediaMetadata.Builder().setTitle(entry.title).build()).build();
    }

    private void play(int index) {
        if (index < 0 || index >= media.size()) return;
        if (controller == null) { pendingIndex = index; return; }
        if (media.get(index).video) {
            controller.setMediaItem(mediaItem(media.get(index)));
            controller.prepare();
            controller.play();
            return;
        }
        ArrayList<MediaItem> items = new ArrayList<>(media.size());
        int start = 0;
        for (int i = 0; i < media.size(); i++) {
            Entry entry = media.get(i);
            if (entry.video) continue;
            if (i == index) start = items.size();
            items.add(mediaItem(entry));
        }
        controller.setMediaItems(items, start, 0);
        controller.prepare();
        controller.play();
    }

    private void openExternal(Intent intent) {
        Uri uri = intent.getData();
        if (uri == null || controller == null) return;
        String title = titleOf(uri);
        if (!canRead(uri)) {
            Toast.makeText(this, "Charter cannot open this file", Toast.LENGTH_LONG).show();
            return;
        }
        controller.setMediaItem(mediaItem(new Entry(uri, title,
                isVideo(uri, title, intent.getType()))));
        controller.prepare();
        controller.play();
        setIntent(new Intent(Intent.ACTION_MAIN));
    }

    private void togglePlayback() {
        if (controller == null) return;
        if (controller.getMediaItemCount() == 0) {
            if (!media.isEmpty()) play(0);
        } else if (controller.isPlaying()) controller.pause();
        else controller.play();
    }

    private void previous() {
        if (controller == null) return;
        if (controller.getCurrentPosition() > 3000) controller.seekTo(0);
        else if (controller.hasPreviousMediaItem()) controller.seekToPreviousMediaItem();
        else controller.seekTo(0);
    }

    private void next() {
        if (controller != null && controller.hasNextMediaItem()) controller.seekToNextMediaItem();
    }

    private void showLibraryActions(int index) {
        Entry entry = media.get(index);
        new AlertDialog.Builder(this).setTitle(entry.title)
                .setItems(new String[] { "Play next", "Add to queue", "Remove from library" },
                        (dialog, choice) -> {
                            if (choice == 2) {
                                media.remove(index);
                                saveLibrary();
                                refreshList();
                            } else if (controller != null) {
                                if (controller.getMediaItemCount() == 0) {
                                    controller.setMediaItem(mediaItem(entry));
                                    controller.prepare();
                                } else if (choice == 0) {
                                    controller.addMediaItem(controller.getCurrentMediaItemIndex() + 1,
                                            mediaItem(entry));
                                } else controller.addMediaItem(mediaItem(entry));
                            }
                        }).show();
    }

    private void showQueueActions(int index) {
        if (controller == null || index >= controller.getMediaItemCount()) return;
        CharSequence title = controller.getMediaItemAt(index).mediaMetadata.title;
        new AlertDialog.Builder(this).setTitle(title == null ? "Queued media" : title)
                .setItems(new String[] { "Remove from queue" },
                        (dialog, choice) -> controller.removeMediaItem(index))
                .show();
    }

    private void refreshList() {
        visibleIndices.clear();
        visibleTitles.clear();
        if (queueMode) {
            if (controller != null) {
                for (int i = 0; i < controller.getMediaItemCount(); i++) {
                    MediaItem item = controller.getMediaItemAt(i);
                    CharSequence title = item.mediaMetadata.title;
                    visibleTitles.add((i == controller.getCurrentMediaItemIndex() ? "▶ " : "") +
                            (title == null ? "Unknown media" : title.toString()));
                }
            }
        } else {
            String query = search.getText().toString().trim().toLowerCase(Locale.ROOT);
            for (int i = 0; i < media.size(); i++) {
                Entry entry = media.get(i);
                if (entry.title.toLowerCase(Locale.ROOT).contains(query)) {
                    visibleIndices.add(i);
                    visibleTitles.add(entry.title);
                }
            }
        }
        adapter.notifyDataSetChanged();
    }

    private void updatePlaybackUi() {
        if (controller == null) return;
        MediaItem item = controller.getCurrentMediaItem();
        if (item == null) {
            nowPlaying.setText("Nothing playing");
            videoView.setVisibility(View.GONE);
            playPause.setText("Play");
        } else {
            CharSequence title = item.mediaMetadata.title;
            String name = title == null ? "Unknown media" : title.toString();
            nowPlaying.setText(name);
            boolean video = false;
            for (Entry entry : media) {
                if (entry.uri.toString().equals(item.mediaId)) { video = entry.video; break; }
            }
            if (!video && item.localConfiguration != null)
                video = isVideo(item.localConfiguration.uri, name, null);
            videoView.setVisibility(video ? View.VISIBLE : View.GONE);
            playPause.setText(controller.isPlaying() ? "Pause" : "Play");
        }
        updateProgress();
    }

    private void updateProgress() {
        if (controller == null) return;
        long duration = Math.max(0, controller.getDuration());
        long position = Math.max(0, controller.getCurrentPosition());
        seek.setMax((int) Math.min(Integer.MAX_VALUE, Math.max(1, duration)));
        if (!seek.isPressed()) seek.setProgress((int) Math.min(Integer.MAX_VALUE, position));
        time.setText(formatTime(position) + " / " + formatTime(duration));
    }

    private String formatTime(long millis) {
        long seconds = millis / 1000;
        long hours = seconds / 3600;
        if (hours > 0) return String.format(Locale.US, "%d:%02d:%02d",
                hours, (seconds / 60) % 60, seconds % 60);
        return String.format(Locale.US, "%d:%02d", seconds / 60, seconds % 60);
    }
}
