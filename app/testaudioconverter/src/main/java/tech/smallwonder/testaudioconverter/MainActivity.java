package tech.smallwonder.testaudioconverter;

import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import androidx.appcompat.app.AppCompatActivity;

import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;

import java.io.File;
import java.util.HashMap;
import java.util.List;

import tech.smallwonder.audioconverter.AudioConverter;

public class MainActivity extends AppCompatActivity {

    private Spinner spinner;
    private Button selectFile, convert;
    private TextView selectedFilePath;

    private AudioConverter audioConverter;
    private int outputFormat = AudioConverter.FORMAT_FLAC;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        spinner = findViewById(R.id.format_options);
        selectFile = findViewById(R.id.select_file_btn);
        selectFile.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
                intent.setType("audio/*");
                startActivityForResult(intent, 200);
            }
        });
        convert = findViewById(R.id.convert_btn);
        convert.setEnabled(false);
        convert.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                Intent intent = new Intent(Intent.ACTION_CREATE_DOCUMENT);
                intent.setType("audio/*");
                startActivityForResult(intent, 300);
            }
        });
        selectedFilePath = findViewById(R.id.selected_file_path);
        spinner.setEnabled(false);
        spinner.setAdapter(ArrayAdapter.createFromResource(this, R.array.formats, android.R.layout.simple_spinner_dropdown_item));
        spinner.setPrompt("Prompt");
        spinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> adapterView, View view, int i, long l) {
                switch (i) {
                    case 0: // FLAC
                        outputFormat = AudioConverter.FORMAT_FLAC;
                        break;
                    case 1: // OPUS
                        outputFormat = AudioConverter.FORMAT_OPUS;
                        break;
                    case 2: // MP3
                        outputFormat = AudioConverter.FORMAT_MP3;
                        break;
                    case 3: // VORBIS
                        outputFormat = AudioConverter.FORMAT_VORBIS;
                        break;
                }
            }

            @Override
            public void onNothingSelected(AdapterView<?> adapterView) {

            }
        });
    }

    @RequiresApi(api = Build.VERSION_CODES.KITKAT)
    @Override
    protected void onActivityResult(int requestCode, int resultCode, @Nullable Intent data) {
        super.onActivityResult(requestCode, resultCode, data);

        if (requestCode == 200 && resultCode == RESULT_OK && data != null) {
            File file = getFileFromURI(data.getData());
            Log.i("MainActivity", "Found path: " + file);
            audioConverter = AudioConverter.create(file.getAbsolutePath());
            if (audioConverter != null) {
                convert.setEnabled(true);
                spinner.setEnabled(true);
                selectedFilePath.setText(file.getAbsolutePath());
            } else {
                convert.setEnabled(false);
                spinner.setEnabled(false);
            }
        } else if (requestCode == 300 && resultCode == RESULT_OK && data != null) {
            final File file = getFileFromURI(data.getData());
            if (file != null) {
                Log.i("MainActivity", "Output path: " + file);
                if (audioConverter != null) {
                    new Thread(new Runnable() {
                        @Override
                        public void run() {
                            new Handler(Looper.getMainLooper()).post(new Runnable() {
                                @Override
                                public void run() {
                                    Toast.makeText(MainActivity.this, "Starting conversion...", Toast.LENGTH_SHORT).show();
                                }
                            });
                            long first = System.currentTimeMillis();
                            if (audioConverter.convert(outputFormat, file, new HashMap<String, String>())) {
                                long second = System.currentTimeMillis();
                                long elapsed = second - first;
                                Log.i("MainActivity", "Took us " + elapsed + " milliseconds!");
                                new Handler(Looper.getMainLooper()).post(new Runnable() {
                                    @Override
                                    public void run() {
                                        Toast.makeText(MainActivity.this, "Conversion successful!", Toast.LENGTH_SHORT).show();
                                    }
                                });
                            } else {
                                new Handler(Looper.getMainLooper()).post(new Runnable() {
                                    @Override
                                    public void run() {
                                        Toast.makeText(MainActivity.this, "Conversion failed!", Toast.LENGTH_SHORT).show();
                                    }
                                });
                            }
                        }
                    }).start();
                }
            }
        }
    }

    @RequiresApi(api = Build.VERSION_CODES.KITKAT)
    private File getFileFromURI(Uri uri) {
        File[] storages = getExternalFilesDirs(null);
        File[] paths = new File[storages.length];
        for (int i = 0; i < paths.length; i++) {
            String url = storages[i].getAbsolutePath();
            String path = url.split("Android")[0];
            paths[i] = new File(path);
        }

        List<String> segments = uri.getPathSegments();
        String[] tokens = segments.get(segments.size() - 1).split(":");
        String finalSegment = tokens[tokens.length - 1];
        File foundPath = null;
        boolean found = false;
        for (File path : paths) {
            foundPath = new File(path, finalSegment);
            if (foundPath.exists()) {
                found = true;
                break;
            }
        }
        if (!found) return null;
        return foundPath;
    }
}
