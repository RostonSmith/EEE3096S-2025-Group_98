import numpy as np
import wave
import matplotlib.pyplot as plt
import os

def wav_to_lut(filename, num_samples_waveform=128, num_samples_audio=8192):
    """
    Extract samples from WAV file optimized for STM32F446 (128KB SRAM)
    - Waveforms: 128 samples (1.5KB total)
    - Audio files: 8192 samples (96KB total) 
    - Total memory: ~97.5KB - leaves plenty of headroom
    """
    
    base_name = os.path.basename(filename).lower()
    is_audio = any(x in base_name for x in ['piano', 'guitar', 'drum'])
    num_samples = num_samples_audio if is_audio else num_samples_waveform
    
    with wave.open(filename, 'r') as wav_file:
        sample_rate = wav_file.getframerate()
        n_frames = wav_file.getnframes()
        sample_width = wav_file.getsampwidth()
        n_channels = wav_file.getnchannels()

        print(f"\n{'='*70}")
        print(f"Processing: {os.path.basename(filename)}")
        print(f"  Type: {'AUDIO' if is_audio else 'WAVEFORM'} - Using {num_samples} samples")
        print(f"  Frames: {n_frames}, Bit-depth: {sample_width*8}, Channels: {n_channels}, Rate: {sample_rate} Hz")
        print(f"  Duration: {n_frames/sample_rate:.2f} seconds")

        audio_data = wav_file.readframes(n_frames)

        # Convert based on sample width
        if sample_width == 1:
            audio_array = np.frombuffer(audio_data, dtype=np.uint8).astype(np.float32) - 128
        elif sample_width == 2:
            audio_array = np.frombuffer(audio_data, dtype=np.int16).astype(np.float32)
        elif sample_width == 3:
            audio_bytes = np.frombuffer(audio_data, dtype=np.uint8)
            audio_array = np.zeros(len(audio_bytes) // 3, dtype=np.float32)
            for i in range(len(audio_array)):
                val = audio_bytes[i*3] | (audio_bytes[i*3+1] << 8) | (audio_bytes[i*3+2] << 16)
                if val >= 0x800000:
                    val -= 0x1000000
                audio_array[i] = float(val)
        elif sample_width == 4:
            audio_array = np.frombuffer(audio_data, dtype=np.int32).astype(np.float32)
        else:
            raise ValueError(f"Unsupported sample width: {sample_width}")

        print(f"  Raw audio array length: {len(audio_array)}")
        print(f"  Raw audio range: [{np.min(audio_array):.1f}, {np.max(audio_array):.1f}]")

        # Convert stereo to mono
        if n_channels == 2:
            audio_array = audio_array.reshape(-1, 2).mean(axis=1)
            print(f"  Converted stereo to mono, new length: {len(audio_array)}")

        max_amplitude = np.max(np.abs(audio_array))
        print(f"  Maximum amplitude: {max_amplitude:.1f}")
        
        if max_amplitude == 0:
            print("  WARNING: Audio appears to be silent!")
            lut_values = np.full(num_samples, 2048, dtype=np.uint32)
            return lut_values

        # Find the best section using combined metrics
        window_size = num_samples
        if len(audio_array) >= window_size:
            metrics = []
            step = max(1, window_size // 32)  # Search more thoroughly
            
            for i in range(0, len(audio_array) - window_size, step):
                window = audio_array[i:i+window_size]
                rms = np.sqrt(np.mean(window**2))
                peak = np.max(np.abs(window))
                variance = np.var(window)
                dynamic_range = np.max(window) - np.min(window)
                zero_crossings = np.sum(np.diff(np.sign(window)) != 0)
                
                # Combined score: emphasize interesting, dynamic sections
                score = (rms * 0.2 + 
                        peak * 0.2 + 
                        np.sqrt(variance) * 0.3 + 
                        np.log1p(dynamic_range) * 0.2 +
                        np.log1p(zero_crossings) * 0.1)
                metrics.append((score, i, rms, peak, variance, dynamic_range))
            
            # Find window with highest combined score
            best_score, best_idx, best_rms, best_peak, best_var, best_dr = max(metrics, key=lambda x: x[0])
            print(f"  Selected section at sample {best_idx} (offset: {best_idx/sample_rate:.3f}s)")
            print(f"    - RMS: {best_rms:.1f}, Peak: {best_peak:.1f}")
            print(f"    - Variance: {best_var:.1f}, Dynamic Range: {best_dr:.1f}")
            samples = audio_array[best_idx:best_idx+num_samples]
        else:
            print(f"  Audio is short, tiling to reach {num_samples} samples")
            samples = np.tile(audio_array, int(np.ceil(num_samples / len(audio_array))))[:num_samples]

        print(f"  Selected samples range: [{np.min(samples):.1f}, {np.max(samples):.1f}]")
        print(f"  Selected samples span: {num_samples/sample_rate*1000:.1f} ms of audio")

        # Normalize to 0-4095 range
        samples_normalized = samples / np.max(np.abs(samples))
        samples_normalized = (samples_normalized + 1.0) / 2.0
        lut_values = (samples_normalized * 4095).astype(np.uint32)
        
        print(f"  Final LUT range: [{np.min(lut_values)}, {np.max(lut_values)}]")
        print(f"  Mean: {np.mean(lut_values):.1f}, Std: {np.std(lut_values):.1f}")
        print(f"  Memory usage: {num_samples * 4:,} bytes ({num_samples * 4 / 1024:.2f} KB)")

        # Enhanced visualization
        fig = plt.figure(figsize=(16, 10))
        gs = fig.add_gridspec(3, 2, hspace=0.3, wspace=0.3)
        
        # Plot 1: Full audio file with selected region highlighted
        ax1 = fig.add_subplot(gs[0, :])
        if len(audio_array) > num_samples * 2:
            downsample_factor = max(1, len(audio_array) // 4000)
            plot_audio = audio_array[::downsample_factor]
            plot_indices = np.arange(0, len(audio_array), downsample_factor)
            ax1.plot(plot_indices / sample_rate, plot_audio, 'gray', linewidth=0.5, alpha=0.7)
            ax1.axvspan(best_idx/sample_rate, (best_idx+num_samples)/sample_rate, 
                       color='red', alpha=0.3, label=f'Selected Region ({num_samples} samples)')
            ax1.set_title(f'Full Audio File: {os.path.basename(filename)}', fontsize=12, fontweight='bold')
            ax1.set_xlabel('Time (seconds)')
            ax1.set_ylabel('Amplitude')
            ax1.legend()
            ax1.grid(True, alpha=0.3)
        
        # Plot 2: Selected audio samples (time domain)
        ax2 = fig.add_subplot(gs[1, 0])
        time_axis = np.arange(num_samples) / sample_rate
        ax2.plot(time_axis * 1000, samples, 'g-', linewidth=1)
        ax2.set_title(f'Selected {num_samples} Samples ({num_samples/sample_rate*1000:.1f}ms)', 
                     fontsize=11, fontweight='bold')
        ax2.set_xlabel('Time (milliseconds)')
        ax2.set_ylabel('Amplitude')
        ax2.grid(True, alpha=0.3)
        ax2.axhline(y=0, color='r', linestyle='--', alpha=0.5)
        
        # Plot 3: Frequency spectrum
        ax3 = fig.add_subplot(gs[1, 1])
        freqs = np.fft.rfftfreq(len(samples), 1/sample_rate)
        spectrum = np.abs(np.fft.rfft(samples))
        ax3.semilogy(freqs[:len(freqs)//4], spectrum[:len(freqs)//4])  # Show up to Nyquist/4
        ax3.set_title('Frequency Spectrum', fontsize=11, fontweight='bold')
        ax3.set_xlabel('Frequency (Hz)')
        ax3.set_ylabel('Magnitude (log scale)')
        ax3.grid(True, alpha=0.3)
        
        # Plot 4: LUT values
        ax4 = fig.add_subplot(gs[2, :])
        ax4.plot(lut_values, 'b-', linewidth=1)
        ax4.axhline(y=2048, color='r', linestyle='--', label='Midpoint (2048)', linewidth=1.5)
        ax4.fill_between(range(len(lut_values)), lut_values, 2048, alpha=0.2)
        ax4.set_title(f'Final LUT Values (0-4095 range) - {len(lut_values)} samples', 
                     fontsize=11, fontweight='bold')
        ax4.set_xlabel('Sample Index')
        ax4.set_ylabel('Value (0-4095)')
        ax4.set_ylim([0, 4095])
        ax4.legend()
        ax4.grid(True, alpha=0.3)
        
        plt.suptitle(f'{os.path.basename(filename)} - Memory: {num_samples*4/1024:.1f}KB', 
                    fontsize=14, fontweight='bold')
        plt.savefig(f'{os.path.basename(filename)}_{num_samples}_analysis.png', 
                   dpi=150, bbox_inches='tight')
        plt.show()

        return lut_values


# Configuration for STM32F446 (128KB SRAM)
NUM_SAMPLES_AUDIO = 8192      # 8K samples = 32KB per audio file = 96KB total for 3 files
NUM_SAMPLES_WAVEFORM = 128    # Keep waveforms small

# Process each file
file_paths = [
    r"C:\Users\rosto\OneDrive - University of Cape Town\Fourth Year\Sem 2\EEE3096S\EEE3096S Git Repo SMTROS022\EEE3096S SMTROS022 Pracs\Prac 4\Misc\Task 1\piano.wav",
    r"C:\Users\rosto\OneDrive - University of Cape Town\Fourth Year\Sem 2\EEE3096S\EEE3096S Git Repo SMTROS022\EEE3096S SMTROS022 Pracs\Prac 4\Misc\Task 1\guitar.wav",
    r"C:\Users\rosto\OneDrive - University of Cape Town\Fourth Year\Sem 2\EEE3096S\EEE3096S Git Repo SMTROS022\EEE3096S SMTROS022 Pracs\Prac 4\Misc\Task 1\drum.wav"
]

print("\n" + "="*70)
print(f"STM32F446 MEMORY USAGE ESTIMATE (128KB SRAM available):")
print(f"  Audio files (3 × {NUM_SAMPLES_AUDIO} samples × 4 bytes):")
print(f"    = {3 * NUM_SAMPLES_AUDIO * 4:,} bytes ({3 * NUM_SAMPLES_AUDIO * 4 / 1024:.2f} KB)")
print(f"  Waveforms (3 × {NUM_SAMPLES_WAVEFORM} samples × 4 bytes):")
print(f"    = {3 * NUM_SAMPLES_WAVEFORM * 4:,} bytes ({3 * NUM_SAMPLES_WAVEFORM * 4 / 1024:.2f} KB)")
print(f"  TOTAL LUT MEMORY: {(3 * NUM_SAMPLES_AUDIO * 4 + 3 * NUM_SAMPLES_WAVEFORM * 4):,} bytes " +
      f"({(3 * NUM_SAMPLES_AUDIO * 4 + 3 * NUM_SAMPLES_WAVEFORM * 4) / 1024:.2f} KB)")
print(f"  Remaining for stack/heap/variables: ~{128 - (3 * NUM_SAMPLES_AUDIO * 4 + 3 * NUM_SAMPLES_WAVEFORM * 4) / 1024:.1f} KB")
print("="*70)

for filepath in file_paths:
    lut = wav_to_lut(filepath, NUM_SAMPLES_WAVEFORM, NUM_SAMPLES_AUDIO)
    
    base_name = os.path.basename(filepath).replace('.wav', '').capitalize()
    
    # Print C array format - optimized for readability
    print(f"\n// {base_name} LUT: {len(lut)} samples, {len(lut)*4:,} bytes ({len(lut)*4/1024:.1f}KB)")
    
    if len(lut) <= NUM_SAMPLES_WAVEFORM:
        print(f"uint32_t {base_name}_LUT[NS_WAVEFORM] = {{")
    else:
        print(f"uint32_t {base_name}_LUT[NS_AUDIO] = {{")
    
    # Print in rows of 16 for readability
    for i in range(0, len(lut), 16):
        values = ', '.join(f"{v:4d}" for v in lut[i:min(i+16, len(lut))])
        if i + 16 < len(lut):
            print(f"    {values},")
        else:
            print(f"    {values}")
    print("};")

print("\n" + "="*70)
print("✓ DONE! Arrays optimized for STM32F446 (128KB SRAM)")
print("  Copy the arrays above into your main.c file")
print("="*70)