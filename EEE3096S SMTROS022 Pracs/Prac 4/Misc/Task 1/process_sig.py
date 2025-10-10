import wave
import numpy as np
import matplotlib.pyplot as plt
import os
from scipy import signal as scipy_signal

def load_wav_mono(path):
    """Load WAV file and convert to mono float32"""
    with wave.open(path, 'rb') as w:
        sr = w.getframerate()
        n = w.getnframes()
        sw = w.getsampwidth()
        ch = w.getnchannels()
        raw = w.readframes(n)
    
    if sw == 1:
        data = np.frombuffer(raw, dtype=np.uint8).astype(np.float32) - 128.0
    elif sw == 2:
        data = np.frombuffer(raw, dtype=np.int16).astype(np.float32)
    elif sw == 3:
        raw_bytes = np.frombuffer(raw, dtype=np.uint8)
        data = np.zeros(len(raw_bytes) // 3, dtype=np.float32)
        for i in range(len(data)):
            val = raw_bytes[i*3] | (raw_bytes[i*3+1] << 8) | (raw_bytes[i*3+2] << 16)
            if val >= 0x800000:
                val -= 0x1000000
            data[i] = float(val)
    elif sw == 4:
        data = np.frombuffer(raw, dtype=np.int32).astype(np.float32)
    else:
        raise ValueError(f"Unsupported sample width: {sw}")
    
    if ch == 2:
        data = data.reshape(-1, 2).mean(axis=1)
    
    return data, sr

def find_best_1second_section(data, sr):
    """
    Find the most interesting 1-second section of audio
    Uses multiple criteria to find the best representative section
    """
    one_second_samples = sr  # 44100 samples for 44.1kHz
    
    if len(data) <= one_second_samples:
        print("  Audio is 1 second or less, using entire file")
        return 0, len(data)
    
    print(f"  Analyzing {len(data)/sr:.1f} seconds of audio to find best 1-second section...")
    
    # Search through the audio
    metrics = []
    step = sr // 10  # Search every 0.1 seconds
    
    for i in range(0, len(data) - one_second_samples, step):
        window = data[i:i+one_second_samples]
        
        # Calculate multiple quality metrics
        rms = np.sqrt(np.mean(window**2))
        peak = np.max(np.abs(window))
        variance = np.var(window)
        dynamic_range = np.max(window) - np.min(window)
        
        # Zero crossings (indicates frequency content)
        zero_crossings = np.sum(np.diff(np.sign(window)) != 0)
        
        # Spectral centroid (brightness of sound)
        spectrum = np.abs(np.fft.rfft(window))
        freqs = np.fft.rfftfreq(len(window), 1/sr)
        spectral_centroid = np.sum(freqs * spectrum) / (np.sum(spectrum) + 1e-10)
        
        # Onset strength (transients, attacks)
        frame_size = 2048
        hop = 512
        onset_strength = 0
        for j in range(0, len(window) - frame_size, hop):
            frame1 = window[j:j+frame_size]
            frame2 = window[j+hop:j+hop+frame_size]
            energy_diff = np.sum(frame2**2) - np.sum(frame1**2)
            if energy_diff > 0:
                onset_strength += energy_diff
        
        # Combined score (weighted)
        score = (
            rms * 0.15 +                              # Loudness
            peak * 0.15 +                             # Peak amplitude
            np.sqrt(variance) * 0.2 +                 # Variation
            np.log1p(dynamic_range) * 0.15 +          # Dynamic range
            np.log1p(zero_crossings) * 0.1 +          # Frequency content
            np.log1p(spectral_centroid) * 0.1 +       # Brightness
            np.log1p(onset_strength) * 0.15           # Transients/interest
        )
        
        metrics.append((score, i, rms, peak, variance, zero_crossings, spectral_centroid))
    
    # Find best section
    best_score, best_idx, best_rms, best_peak, best_var, best_zc, best_sc = max(metrics, key=lambda x: x[0])
    
    print(f"  Best 1-second section found at {best_idx/sr:.2f}s - {(best_idx+one_second_samples)/sr:.2f}s")
    print(f"    RMS: {best_rms:.1f}, Peak: {best_peak:.1f}, Variance: {best_var:.1f}")
    print(f"    Zero crossings: {best_zc}, Spectral centroid: {best_sc:.1f}Hz")
    
    return best_idx, one_second_samples

def analyze_optimal_resolution(data, sr):
    """
    Analyze the 1-second section to determine optimal resolution
    Tests different resolutions and calculates quality metrics
    """
    print("\n  Analyzing optimal resolution...")
    
    # Test resolutions
    test_resolutions = [8192]
    
    results = []
    
    for N in test_resolutions:
        # Downsample using energy-adaptive method
        resampled = energy_adaptive_resample(data, N)
        
        # Calculate quality metrics
        
        # 1. Correlation with original (downsampled)
        orig_downsampled = scipy_signal.resample(data, N)
        correlation = np.corrcoef(resampled, orig_downsampled)[0, 1]
        
        # 2. Frequency content preservation
        orig_fft = np.abs(np.fft.rfft(data))
        resamp_fft = np.abs(np.fft.rfft(resampled))
        # Compare first N/2 frequency bins
        freq_bins_to_compare = min(len(orig_fft), len(resamp_fft)) // 2
        orig_fft_norm = orig_fft[:freq_bins_to_compare] / (np.sum(orig_fft[:freq_bins_to_compare]) + 1e-10)
        resamp_fft_norm = resamp_fft[:freq_bins_to_compare] / (np.sum(resamp_fft[:freq_bins_to_compare]) + 1e-10)
        freq_similarity = 1 - np.sum(np.abs(orig_fft_norm - resamp_fft_norm))
        
        # 3. Dynamic range preservation
        orig_dr = np.max(data) - np.min(data)
        resamp_dr = np.max(resampled) - np.min(resampled)
        dr_ratio = min(orig_dr, resamp_dr) / (max(orig_dr, resamp_dr) + 1e-10)
        
        # 4. Memory efficiency score (favor smaller sizes)
        memory_kb = N * 4 / 1024
        memory_score = 1.0 / (1.0 + memory_kb / 10)  # Normalized
        
        # Combined quality score
        quality_score = (
            correlation * 0.35 +
            freq_similarity * 0.35 +
            dr_ratio * 0.15 +
            memory_score * 0.15
        )
        
        results.append({
            'N': N,
            'memory_kb': memory_kb,
            'correlation': correlation,
            'freq_similarity': freq_similarity,
            'dr_ratio': dr_ratio,
            'quality_score': quality_score
        })
        
        print(f"    {N:5d} samples: Quality={quality_score:.3f}, Correlation={correlation:.3f}, "
              f"Freq={freq_similarity:.3f}, Memory={memory_kb:.1f}KB")
    
    # Find optimal resolution
    # Prioritize quality but consider memory
    best = max(results, key=lambda x: x['quality_score'])
    
    print(f"\n  RECOMMENDED: {best['N']} samples (Quality={best['quality_score']:.3f}, {best['memory_kb']:.1f}KB)")
    
    return results, best['N']

def energy_adaptive_resample(x, N, eps=1e-12):
    """Energy-adaptive resampling - more samples where energy is high"""
    x = x - np.mean(x)
    energy = x**2 + eps
    
    # Smooth energy
    window = max(1, int(len(x) / 500))
    kernel = np.ones(window) / window
    energy_sm = np.convolve(energy, kernel, mode='same')
    
    # CDF
    cdf = np.cumsum(energy_sm)
    cdf = (cdf - cdf[0]) / (cdf[-1] - cdf[0])
    
    # Sample uniformly in energy space
    targets = np.linspace(0.0, 1.0, N)
    idx = np.interp(targets, cdf, np.arange(len(cdf)))
    res = np.interp(idx, np.arange(len(x)), x)
    
    return res

def to_12bit_lut(arr):
    """Convert to 12-bit DAC values (0-4095)"""
    if np.max(np.abs(arr)) == 0:
        norm = np.zeros_like(arr)
    else:
        norm = arr / np.max(np.abs(arr))
    
    scaled = np.round((norm + 1.0) / 2.0 * 4095.0).astype(np.uint32)
    scaled = np.clip(scaled, 0, 4095)
    
    return scaled

def export_c_array(name, arr, define_name="NS"):
    """Export as C array"""
    base_name = os.path.splitext(os.path.basename(name))[0]
    clean_name = ''.join(c if c.isalnum() else '_' for c in base_name).capitalize()
    
    out = f"// {clean_name}: {len(arr)} samples, {len(arr)*4:,} bytes ({len(arr)*4/1024:.2f}KB)\n"
    out += f"uint32_t {clean_name}_LUT[{define_name}] = {{\n"
    
    for i in range(0, len(arr), 16):
        chunk = ', '.join(f"{int(x):4d}" for x in arr[i:i+16])
        if i + 16 < len(arr):
            out += f"    {chunk},\n"
        else:
            out += f"    {chunk}\n"
    
    out += "};\n"
    return out

def process_wav_with_analysis(path, target_resolution=None, show_plots=True):
    """
    Complete processing pipeline:
    1. Load audio
    2. Find best 1-second section
    3. Analyze optimal resolution (if not specified)
    4. Resample and generate LUT
    """
    data, sr = load_wav_mono(path)
    
    print(f"\n{'='*70}")
    print(f"Processing: {os.path.basename(path)}")
    print(f"  Total duration: {len(data)/sr:.2f}s ({len(data):,} samples at {sr}Hz)")
    
    # Find best 1-second section
    best_idx, section_length = find_best_1second_section(data, sr)
    one_second_section = data[best_idx:best_idx+section_length]
    
    print(f"  Extracted 1-second section: {len(one_second_section):,} samples")
    
    # Analyze optimal resolution if not specified
    if target_resolution is None:
        analysis_results, recommended_N = analyze_optimal_resolution(one_second_section, sr)
        target_resolution = recommended_N
    else:
        print(f"\n  Using specified resolution: {target_resolution} samples")
        analysis_results = None
    
    # Resample to target resolution
    print(f"\n  Resampling to {target_resolution} samples...")
    resampled = energy_adaptive_resample(one_second_section, target_resolution)
    
    print(f"  Compression ratio: {len(one_second_section)/target_resolution:.1f}x")
    print(f"  Resampled range: [{np.min(resampled):.1f}, {np.max(resampled):.1f}]")
    
    # Convert to LUT
    lut = to_12bit_lut(resampled)
    
    print(f"  LUT range: [{np.min(lut)}, {np.max(lut)}]")
    print(f"  Mean: {np.mean(lut):.1f}, Std: {np.std(lut):.1f}")
    print(f"  Memory: {len(lut)*4:,} bytes ({len(lut)*4/1024:.2f}KB)")
    
    if show_plots:
        # Create comprehensive visualization
        if analysis_results is not None:
            fig = plt.figure(figsize=(18, 14))
            gs = fig.add_gridspec(6, 2, hspace=0.4, wspace=0.3)
        else:
            fig = plt.figure(figsize=(18, 12))
            gs = fig.add_gridspec(5, 2, hspace=0.4, wspace=0.3)
        
        # Plot 1: Full audio with selected 1-second section
        ax1 = fig.add_subplot(gs[0, :])
        downsample = max(1, len(data) // 5000)
        plot_data = data[::downsample]
        plot_time = np.arange(len(plot_data)) * downsample / sr
        ax1.plot(plot_time, plot_data, 'gray', linewidth=0.5, alpha=0.7)
        ax1.axvspan(best_idx/sr, (best_idx+section_length)/sr, 
                   color='red', alpha=0.3, label='Selected 1-second section')
        ax1.set_title(f'Full Audio: {os.path.basename(path)} ({len(data)/sr:.2f}s)', 
                     fontsize=12, fontweight='bold')
        ax1.set_xlabel('Time (s)')
        ax1.set_ylabel('Amplitude')
        ax1.legend()
        ax1.grid(True, alpha=0.3)
        
        # Plot 2: Selected 1-second section waveform
        ax2 = fig.add_subplot(gs[1, 0])
        section_time = np.arange(len(one_second_section)) / sr
        downsample2 = max(1, len(one_second_section) // 2000)
        ax2.plot(section_time[::downsample2], one_second_section[::downsample2], 
                'g-', linewidth=0.8)
        ax2.set_title('Selected 1-Second Section (Time Domain)', fontsize=11, fontweight='bold')
        ax2.set_xlabel('Time (s)')
        ax2.set_ylabel('Amplitude')
        ax2.grid(True, alpha=0.3)
        ax2.axhline(0, color='r', linestyle='--', alpha=0.5)
        
        # Plot 3: Spectrogram of 1-second section
        ax3 = fig.add_subplot(gs[1, 1])
        f, t, Sxx = scipy_signal.spectrogram(one_second_section, sr, nperseg=1024)
        pcm = ax3.pcolormesh(t, f, 10 * np.log10(Sxx + 1e-10), shading='gouraud', cmap='viridis')
        ax3.set_title('Spectrogram (1-Second Section)', fontsize=11, fontweight='bold')
        ax3.set_ylabel('Frequency (Hz)')
        ax3.set_xlabel('Time (s)')
        ax3.set_ylim([0, min(sr/2, 8000)])
        plt.colorbar(pcm, ax=ax3, label='Power (dB)')
        
        # Plot 4: Energy distribution
        ax4 = fig.add_subplot(gs[2, 0])
        energy = one_second_section**2
        window = max(1, int(len(energy) / 500))
        kernel = np.ones(window) / window
        energy_smooth = np.convolve(energy, kernel, mode='same')
        energy_time = np.arange(len(energy_smooth)) / sr
        downsample3 = max(1, len(energy_smooth) // 2000)
        ax4.plot(energy_time[::downsample3], energy_smooth[::downsample3], 
                'purple', linewidth=1.5)
        ax4.set_title('Energy Distribution (guides resampling)', fontsize=11, fontweight='bold')
        ax4.set_xlabel('Time (s)')
        ax4.set_ylabel('Energy')
        ax4.grid(True, alpha=0.3)
        ax4.fill_between(energy_time[::downsample3], energy_smooth[::downsample3], alpha=0.3)
        
        # Plot 5: Resampled waveform
        ax5 = fig.add_subplot(gs[2, 1])
        ax5.plot(resampled, 'b-', linewidth=1, alpha=0.8)
        ax5.set_title(f'Resampled Signal ({target_resolution} samples)', 
                     fontsize=11, fontweight='bold')
        ax5.set_xlabel('Sample Index')
        ax5.set_ylabel('Amplitude')
        ax5.grid(True, alpha=0.3)
        ax5.axhline(0, color='r', linestyle='--', alpha=0.5)
        
        # Plot 6: Frequency comparison
        ax6 = fig.add_subplot(gs[3, 0])
        orig_fft = np.abs(np.fft.rfft(one_second_section))
        orig_freqs = np.fft.rfftfreq(len(one_second_section), 1/sr)
        resamp_fft = np.abs(np.fft.rfft(resampled))
        resamp_freqs = np.fft.rfftfreq(len(resampled), 1/(sr * len(resampled) / len(one_second_section)))
        
        # Plot up to 8kHz
        orig_mask = orig_freqs <= 8000
        resamp_mask = resamp_freqs <= 8000
        
        ax6.semilogy(orig_freqs[orig_mask], orig_fft[orig_mask], 
                    'g-', alpha=0.7, linewidth=1, label='Original 1s section')
        ax6.semilogy(resamp_freqs[resamp_mask], resamp_fft[resamp_mask] * (len(one_second_section)/len(resampled)), 
                    'b-', alpha=0.7, linewidth=1.5, label=f'Resampled ({target_resolution})')
        ax6.set_title('Frequency Spectrum Comparison', fontsize=11, fontweight='bold')
        ax6.set_xlabel('Frequency (Hz)')
        ax6.set_ylabel('Magnitude')
        ax6.legend()
        ax6.grid(True, alpha=0.3)
        
        # Plot 7: Final LUT
        row_idx = 4 if analysis_results is None else 5
        ax7 = fig.add_subplot(gs[3, 1])
        ax7.plot(lut, 'b-', linewidth=1.5)
        ax7.axhline(2048, color='r', linestyle='--', label='Midpoint (2048)', linewidth=1.5)
        ax7.fill_between(range(len(lut)), lut, 2048, alpha=0.2)
        ax7.set_title(f'Final 12-bit LUT', fontsize=11, fontweight='bold')
        ax7.set_xlabel('Sample Index')
        ax7.set_ylabel('Value (0-4095)')
        ax7.set_ylim([0, 4095])
        ax7.legend()
        ax7.grid(True, alpha=0.3)
        
        # Plot 8: Quality analysis (if available)
        if analysis_results is not None:
            ax8 = fig.add_subplot(gs[4, :])
            resolutions = [r['N'] for r in analysis_results]
            quality = [r['quality_score'] for r in analysis_results]
            memory = [r['memory_kb'] for r in analysis_results]
            
            ax8_twin = ax8.twinx()
            
            line1 = ax8.plot(resolutions, quality, 'bo-', linewidth=2, markersize=8, label='Quality Score')
            ax8.axvline(target_resolution, color='r', linestyle='--', linewidth=2, alpha=0.7, label='Selected')
            ax8.set_xlabel('Resolution (samples)', fontsize=11)
            ax8.set_ylabel('Quality Score', color='b', fontsize=11)
            ax8.tick_params(axis='y', labelcolor='b')
            ax8.set_xscale('log', base=2)
            ax8.grid(True, alpha=0.3)
            
            line2 = ax8_twin.plot(resolutions, memory, 'gs-', linewidth=2, markersize=8, label='Memory (KB)')
            ax8_twin.set_ylabel('Memory (KB)', color='g', fontsize=11)
            ax8_twin.tick_params(axis='y', labelcolor='g')
            
            # Combined legend
            lines = line1 + line2
            labels = [l.get_label() for l in lines]
            ax8.legend(lines, labels, loc='upper left')
            
            ax8.set_title('Quality vs Memory Trade-off Analysis', fontsize=11, fontweight='bold')
        
        # Plot 9: Sample distribution visualization
        ax9 = fig.add_subplot(gs[row_idx, :])
        
        # Show where samples were taken from original
        x = one_second_section - np.mean(one_second_section)
        energy = x**2 + 1e-12
        window = max(1, int(len(x) / 500))
        kernel = np.ones(window) / window
        energy_sm = np.convolve(energy, kernel, mode='same')
        cdf = np.cumsum(energy_sm)
        cdf = (cdf - cdf[0]) / (cdf[-1] - cdf[0])
        targets = np.linspace(0.0, 1.0, target_resolution)
        sample_indices = np.interp(targets, cdf, np.arange(len(cdf)))
        
        downsample4 = max(1, len(one_second_section) // 2000)
        ax9.plot(np.arange(len(one_second_section))[::downsample4] / sr, 
                one_second_section[::downsample4], 'gray', alpha=0.5, linewidth=0.5, label='Original')
        ax9.scatter(sample_indices / sr, resampled, c='red', s=10, alpha=0.6, 
                   label=f'Sampled points ({target_resolution})', zorder=5)
        ax9.set_title('Sample Distribution (Energy-Adaptive)', fontsize=11, fontweight='bold')
        ax9.set_xlabel('Time (s)')
        ax9.set_ylabel('Amplitude')
        ax9.legend()
        ax9.grid(True, alpha=0.3)
        
        plt.suptitle(f'{os.path.basename(path)} - {target_resolution} samples, {target_resolution*4/1024:.2f}KB', 
                    fontsize=14, fontweight='bold')
        
        save_name = f"{os.path.splitext(os.path.basename(path))[0]}_{target_resolution}_1sec_analysis.png"
        plt.savefig(save_name, dpi=150, bbox_inches='tight')
        print(f"  Plot saved: {save_name}")
        plt.show()
    
    return lut, target_resolution, analysis_results

# ============================================================================
# MAIN EXECUTION
# ============================================================================
if __name__ == '__main__':
    
    file_paths = [
        r"C:\Users\rosto\OneDrive - University of Cape Town\Fourth Year\Sem 2\EEE3096S\EEE3096S Git Repo SMTROS022\EEE3096S SMTROS022 Pracs\Prac 4\Misc\Task 1\piano.wav",
        r"C:\Users\rosto\OneDrive - University of Cape Town\Fourth Year\Sem 2\EEE3096S\EEE3096S Git Repo SMTROS022\EEE3096S SMTROS022 Pracs\Prac 4\Misc\Task 1\guitar.wav",
        r"C:\Users\rosto\OneDrive - University of Cape Town\Fourth Year\Sem 2\EEE3096S\EEE3096S Git Repo SMTROS022\EEE3096S SMTROS022 Pracs\Prac 4\Misc\Task 1\drum.wav"
    ]
    
    # Option 1: AUTO-DETECT optimal resolution for each file
    print("\n" + "="*70)
    print("MODE: AUTO-DETECT OPTIMAL RESOLUTION")
    print("  Analyzing 1-second sections to find best resolution for each file")
    print("="*70)
    
    results_auto = {}
    all_analysis_results = {}
    
    for filepath in file_paths:
        if os.path.exists(filepath):
            lut, used_resolution, analysis = process_wav_with_analysis(filepath, target_resolution=None, show_plots=True)
            results_auto[filepath] = (lut, used_resolution)
            all_analysis_results[filepath] = analysis
    
    # ========================================================================
    # SAVE TO TEXT FILES
    # ========================================================================
    
    # Save individual C arrays to separate files
    print("\n" + "="*70)
    print("SAVING C ARRAYS TO TEXT FILES...")
    print("="*70)
    
    for filepath, (lut, N) in results_auto.items():
        base_name = os.path.splitext(os.path.basename(filepath))[0]
        output_filename = f"{base_name}_LUT_{N}samples.txt"
        
        with open(output_filename, 'w') as f:
            f.write("="*70 + "\n")
            f.write(f"C ARRAY FOR {base_name.upper()}\n")
            f.write(f"Generated from: {os.path.basename(filepath)}\n")
            f.write(f"Resolution: {N} samples ({N*4/1024:.2f}KB)\n")
            f.write("="*70 + "\n\n")
            
            f.write(f"// Add this define to your main.c\n")
            f.write(f"#define NS_{base_name.upper()}  {N}\n\n")
            
            f.write(export_c_array(filepath, lut, f"NS_{base_name.upper()}"))
            
            f.write("\n" + "="*70 + "\n")
            f.write("USAGE INSTRUCTIONS:\n")
            f.write("="*70 + "\n")
            f.write("1. Copy the #define line to the /* USER CODE BEGIN PD */ section\n")
            f.write("2. Copy the uint32_t array to the /* USER CODE BEGIN PV */ section\n")
            f.write("3. Update your code to use this LUT\n")
        
        print(f"  ✓ Saved: {output_filename}")
    
    # Save combined file with all arrays
    combined_filename = "ALL_LUTS_combined.txt"
    with open(combined_filename, 'w') as f:
        f.write("="*70 + "\n")
        f.write("ALL AUDIO LUTs - COMBINED FILE\n")
        f.write("="*70 + "\n\n")
        
        f.write("// STEP 1: Add these defines to your main.c in /* USER CODE BEGIN PD */\n")
        f.write("="*70 + "\n\n")
        
        for filepath, (lut, N) in results_auto.items():
            base_name = os.path.splitext(os.path.basename(filepath))[0]
            f.write(f"#define NS_{base_name.upper()}  {N}  // {N*4/1024:.2f}KB\n")
        
        total_memory = sum(N * 4 / 1024 for _, (_, N) in results_auto.items())
        f.write(f"\n// Total memory for audio LUTs: {total_memory:.2f}KB\n")
        f.write(f"// Plus waveforms (3 x 1024 x 4 bytes): 12.00KB\n")
        f.write(f"// GRAND TOTAL: {total_memory + 12:.2f}KB\n\n")
        
        f.write("="*70 + "\n")
        f.write("// STEP 2: Add these arrays to your main.c in /* USER CODE BEGIN PV */\n")
        f.write("="*70 + "\n\n")
        
        for filepath, (lut, N) in results_auto.items():
            base_name = os.path.splitext(os.path.basename(filepath))[0]
            f.write(export_c_array(filepath, lut, f"NS_{base_name.upper()}"))
            f.write("\n")
        
        f.write("="*70 + "\n")
        f.write("MEMORY USAGE SUMMARY\n")
        f.write("="*70 + "\n\n")
        
        for filepath, (lut, N) in results_auto.items():
            base_name = os.path.splitext(os.path.basename(filepath))[0]
            f.write(f"{base_name.capitalize():10s}: {N:5d} samples = {N*4:6d} bytes ({N*4/1024:6.2f}KB)\n")
        
        f.write(f"\nTotal Audio:     {total_memory:6.2f}KB\n")
        f.write(f"Waveforms:       12.00KB (3 x 1024 samples)\n")
        f.write(f"GRAND TOTAL:     {total_memory + 12:6.2f}KB\n")
        f.write(f"Available SRAM:  128.00KB (STM32F446)\n")
        f.write(f"Remaining:       {128 - total_memory - 12:6.2f}KB\n")
    
    print(f"  ✓ Saved: {combined_filename}")
    
    # Save analysis report