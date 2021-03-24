package tech.smallwonder.audioconverter;

import android.provider.MediaStore;
import android.util.Log;

import java.io.File;
import java.util.Map;

public class AudioConverter {
    static {
        System.loadLibrary("audioconvert");
    }

    public static final int FORMAT_FLAC = 86028;
    public static final int FORMAT_OPUS = 86076;
    public static final int FORMAT_MP3 = 86017;
    public static final int FORMAT_VORBIS = 86021;

    /**
     * Creates a new AudioConverter.
     * @param url - Path to the file to convert
     * @return a valid AudioConverter if this url is valid and null otherwise
     */
    public static AudioConverter create(String url) {
        AudioConverter converter = new AudioConverter(url);
        if (converter.id == -1) {
            Log.i("AudioConverter", "Could not create converter!");
        }
        if (converter.id == -1 || !converter.initializeNative(converter.id)) return null;
        return converter;
    }

    private long id;

    /**
     * Create a new AudioConverter instance
     * @param url - Path to input file
     */
    private AudioConverter(String url) {
        id = newAudioConverterNative(url);
    }

    private native long newAudioConverterNative(String url);
    private native boolean initializeNative(long id);
    private native boolean convertNative(long id, int outputFormat, String outputPath, String[] metadata_keys, String[] metadata_values);
    private native int getConversionProgress(long id);
    private native void releaseNative(long id);

    /**
     * Please make sure the metadata parameter is not null
     * @return true if the conversion operation succeeded or false otherwise
     */
    public boolean convert(int outputFormat, File outputPath, Map<String, String> metadata) {
        String[] keys = new String[metadata.size()];
        String[] values = new String[metadata.size()];
        keys = metadata.keySet().toArray(keys);
        values = metadata.values().toArray(values);
        String path = outputPath.getAbsolutePath();
        if (outputFormat == FORMAT_MP3) {
            if (!path.endsWith(".mp3")) {
                path = path.concat(".mp3");
                File file = new File(path);
                outputPath.renameTo(file);
                outputPath = file;
            }
        } else if (outputFormat == FORMAT_VORBIS) {
            if (!path.endsWith(".ogg")) {
                path = path.concat(".ogg");
                File file = new File(path);
                outputPath.renameTo(file);
                outputPath = file;
            }
        } else if (outputFormat == FORMAT_FLAC) {
            if (!path.endsWith(".flac")) {
                path = path.concat(".flac");
                File file = new File(path);
                outputPath.renameTo(file);
                outputPath = file;
            }
        } else if (outputFormat == FORMAT_OPUS) {
            if (!path.endsWith(".ogg")) {
                path = path.concat(".ogg");
                File file = new File(path);
                outputPath.renameTo(file);
                outputPath = file;
            }
        } else {
            Log.d("AudioConverter", "No known format specified!");
            return false;
        }
        return convertNative(id, outputFormat, outputPath.getAbsolutePath(), keys, values);
    }

    /**
     * Returns the conversion progress if a conversion is currently running or zero if no conversion is in progress
     */
    public int getConversionProgress() {
        return getConversionProgress(id);
    }

    @Override
    protected void finalize() throws Throwable {
        super.finalize();
        releaseNative(id);
    }
}
