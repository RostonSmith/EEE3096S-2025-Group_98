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

def detect_key_regions(data, sr, min_silence_duration_ms=50):
    """
    Detect key regions of audio by finding non-silent sections
    Returns list of (start_idx, end_idx) tuples
    """
    # Calculate envelope (smoothed absolute value)
    window_size = int(sr * 0.01)  # 10ms window
    envelope = np.convolve(np.abs(data), np.ones(window_size)/window_size, mode='same')
    
    # Adaptive threshold based on signal statistics
    threshold = np.mean(envelope) + 0.5 * np.std(envelope)
    threshold = max(threshold, np.max(envelope) * 0.05)  # At least 5% of peak
    
    # Find regions above threshold
    above_threshold = envelope > threshold
    
    # Find transitions
    transitions = np.diff(above_threshold.astype(int))
    starts = np.where(transitions == 1)[0]
    ends = np.where(transitions == -1)[0]
    
    # Handle edge cases
    if above_threshold[0]:
        starts = np.concatenate([[0], starts])
    if above_threshold[-1]:
        ends = np.concatenate([ends, [len(data)-1]])
    
    # Merge close regions
    min_silence_samples = int(sr * min_silence_duration_ms / 1000)
    merged_regions = []
    
    if len(starts) > 0 and len(ends) > 0:
        current_start = starts[0]
        current_end = ends[0]
        
        for i in range(1, len(starts)):
            if starts[i] - current_end < min_silence_samples:
                # Merge with current region
                current_end = ends[i]
            else:
                # Save current region and start new one
                merged_regions.append((current_start, current_end))
                current_start = starts[i]
                current_end = ends[i]
        
        merged_regions.append((current_start, current_end))
    
    return merged_regions, envelope, threshold

def extract_key_samples(data, sr, N_target, strategy='mixed'):
    """
    Extract N_target samples from entire audio intelligently
    
    Strategies:
    - 'energy': Pure energy-adaptive across whole file
    - 'events': Focus on detected events/transients
    - 'mixed': Combination (recommended)
    """
    print(f"  Strategy: {strategy}")
    
    if strategy == 'energy':
        # Energy-adaptive across entire signal
        return energy_adaptive_resample_whole(data, N_target)
    
    elif strategy == 'events':
        # Detect and sample from key events
        return event_based_sampling(data, sr, N_target)
    
    elif strategy == 'mixed':
        # Hybrid approach
        regions, envelope, threshold = detect_key_regions(data, sr)
        
        if len(regions) == 0:
            print("  No distinct regions detected, using energy-adaptive")
            return energy_adaptive_resample_whole(data, N_target)
        
        # Calculate importance of each region
        region_importance = []
        total_energy = 0
        
        for start, end in regions:
            region_data = data[start:end]
            energy = np.sum(region_data**2)
            peak = np.max(np.abs(region_data))
            duration = end - start
            
            # Importance score
            score = energy * 0.6 + peak * duration * 0.4
            region_importance.append(score)
            total_energy += score
        
        region_importance = np.array(region_importance)
        
        # Allocate samples proportionally to importance
        samples_per_region = (region_importance / total_energy * N_target).astype(int)
        
        # Ensure we use all samples
        while np.sum(samples_per_region) < N_target:
            idx = np.argmax(region_importance)
            samples_per_region[idx] += 1
        
        while np.sum(samples_per_region) > N_target:
            idx = np.argmin(samples_per_region)
            if samples_per_region[idx] > 1:
                samples_per_region[idx] -= 1
            else:
                break
        
        print(f"  Detected {len(regions)} key regions")
        
        # Sample from each region
        result_samples = []
        for i, (start, end) in enumerate(regions):
            n_samples = samples_per_region[i]
            if n_samples < 1:
                continue
            
            region_data = data[start:end]
            region_samples = energy_adaptive_resample_whole(region_data, n_samples)
            result_samples.append(region_samples)
            
            duration_ms = (end - start) / sr * 1000
            print(f"    Region {i+1}: {start}-{end} ({duration_ms:.0f}ms) -> {n_samples} samples")
        
        # Concatenate all samples
        if len(result_samples) > 0:
            return np.concatenate(result_samples)
        else:
            return energy_adaptive_resample_whole(data, N_target)
    
    else:
        raise ValueError(f"Unknown strategy: {strategy}")

def energy_adaptive_resample_whole(x, N, eps=1e-12):
    """Energy-adaptive resampling for entire signal"""
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

def event_based_sampling(data, sr, N_target):
    """
    Sample based on detected events (onsets, transients)
    Good for percussive or rhythmic content
    """
    # Detect onset strength
    hop_length = max(1, len(data) // 10000)
    
    # Simple onset detection using energy difference
    frame_length = int(sr * 0.02)  # 20ms frames
    frames = []
    
    for i in range(0, len(data) - frame_length, hop_length):
        frame = data[i:i+frame_length]
        energy = np.sum(frame**2)
        frames.append(energy)
    
    frames = np.array(frames)
    
    # Onset strength = difference in energy
    onset_strength = np.diff(frames)
    onset_strength = np.concatenate([[0], onset_strength])
    onset_strength[onset_strength < 0] = 0  # Only increases
    
    # Smooth
    if len(onset_strength) > 10:
        onset_strength = np.convolve(onset_strength, np.ones(5)/5, mode='same')
    
    # Create importance map back to sample space
    importance = np.zeros(len(data))
    for i, strength in enumerate(onset_strength):
        start_idx = i * hop_length
        end_idx = min(start_idx + hop_length, len(data))
        importance[start_idx:end_idx] = strength
    
    # Add baseline energy importance
    baseline = data**2
    importance = importance * 0.7 + baseline * 0.3
    
    # Sample based on importance
    importance[importance < 0] = 0
    importance += 1e-12
    
    cdf = np.cumsum(importance)
    cdf = (cdf - cdf[0]) / (cdf[-1] - cdf[0])
    
    targets = np.linspace(0.0, 1.0, N_target)
    idx = np.interp(targets, cdf, np.arange(len(cdf)))
    result = np.interp(idx, np.arange(len(data)), data)
    
    return result

def to_12bit_lut(arr):
    """Convert to 12-bit DAC values (0-4095)"""
    if np.max(np.abs(arr)) == 0:
        norm = np.zeros_like(arr)
    else:
        norm = arr / np.max(np.abs(arr))
    
    scaled = np.round((norm + 1.0) / 2.0 * 4095.0).astype(np.uint32)
    scaled = np.clip(scaled, 0, 4095)
    
    return scaled

def export_c_array(name, arr, array_size_define="NS_AUDIO"):
    """Export as C array"""
    base_name = os.path.splitext(os.path.basename(name))[0]
    clean_name = ''.join(c if c.isalnum() else '_' for c in base_name).capitalize()
    
    out = f"// {clean_name}: {len(arr)} samples, {len(arr)*4:,} bytes ({len(arr)*4/1024:.2f}KB)\n"
    out += f"uint32_t {clean_name}_LUT[{array_size_define}] = {{\n"
    
    for i in range(0, len(arr), 16):
        chunk = ', '.join(f"{int(x):4d}" for x in arr[i:i+16])
        if i + 16 < len(arr):
            out += f"    {chunk},\n"
        else:
            out += f"    {chunk}\n"
    
    out += "};\n"
    return out

def process_full_audio(path, N_samples, strategy='mixed', show_plots=True):
    """
    Process ENTIRE audio file into N_samples
    """
    data, sr = load_wav_mono(path)
    
    print(f"\n{'='*70}")
    print(f"Processing: {os.path.basename(path)}")
    print(f"  Full length: {len(data):,} samples ({len(data)/sr:.2f}s at {sr}Hz)")
    print(f"  Compression ratio: {len(data)/N_samples:.1f}x")
    print(f"  Original size: {len(data)*4/1024/1024:.2f}MB")
    print(f"  Target size: {N_samples*4/1024:.2f}KB")
    
    # Extract key samples from ENTIRE file
    resampled = extract_key_samples(data, sr, N_samples, strategy=strategy)
    
    print(f"  Extracted {len(resampled)} samples")
    print(f"  Range: [{np.min(resampled):.1f}, {np.max(resampled):.1f}]")
    
    # Convert to LUT
    lut = to_12bit_lut(resampled)
    
    print(f"  LUT range: [{np.min(lut)}, {np.max(lut)}]")
    print(f"  Mean: {np.mean(lut):.1f}, Std: {np.std(lut):.1f}")
    
    if show_plots:
        # Visualization
        fig = plt.figure(figsize=(16, 12))
        gs = fig.add_gridspec(5, 2, hspace=0.4, wspace=0.3)
        
        # Plot 1: Full original audio
        ax1 = fig.add_subplot(gs[0, :])
        downsample = max(1, len(data) // 5000)
        plot_data = data[::downsample]
        plot_time = np.arange(len(plot_data)) * downsample / sr
        ax1.plot(plot_time, plot_data, 'gray', linewidth=0.5, alpha=0.7)
        ax1.set_title(f'Original Audio: {os.path.basename(path)} ({len(data)/sr:.2f}s)', 
                     fontsize=12, fontweight='bold')
        ax1.set_xlabel('Time (s)')
        ax1.set_ylabel('Amplitude')
        ax1.grid(True, alpha=0.3)
        
        # Plot 2: Envelope with detected regions
        ax2 = fig.add_subplot(gs[1, :])
        regions, envelope, threshold = detect_key_regions(data, sr)
        env_downsample = max(1, len(envelope) // 5000)
        ax2.plot(np.arange(len(envelope))[::env_downsample] / sr, 
                envelope[::env_downsample], 'b-', linewidth=1, alpha=0.7, label='Envelope')
        ax2.axhline(threshold, color='r', linestyle='--', label=f'Threshold', alpha=0.7)
        
        # Highlight regions
        for i, (start, end) in enumerate(regions[:20]):  # Show first 20
            ax2.axvspan(start/sr, end/sr, color='green', alpha=0.2)
        
        ax2.set_title(f'Signal Envelope & Key Regions (detected {len(regions)} regions)', 
                     fontsize=11, fontweight='bold')
        ax2.set_xlabel('Time (s)')
        ax2.set_ylabel('Envelope')
        ax2.legend()
        ax2.grid(True, alpha=0.3)
        
        # Plot 3: Energy distribution of original
        ax3 = fig.add_subplot(gs[2, 0])
        energy = data**2
        energy_downsample = max(1, len(energy) // 2000)
        ax3.plot(np.arange(len(energy))[::energy_downsample] / sr,
                energy[::energy_downsample], 'purple', linewidth=1)
        ax3.set_title('Energy Distribution (Original)', fontsize=11, fontweight='bold')
        ax3.set_xlabel('Time (s)')
        ax3.set_ylabel('Energy')
        ax3.grid(True, alpha=0.3)
        ax3.fill_between(np.arange(len(energy))[::energy_downsample] / sr,
                        energy[::energy_downsample], alpha=0.3)
        
        # Plot 4: Resampled waveform
        ax4 = fig.add_subplot(gs[2, 1])
        ax4.plot(resampled, 'g-', linewidth=1, alpha=0.8)
        ax4.set_title(f'Condensed Signal ({N_samples} samples, {len(data)/N_samples:.0f}x compression)', 
                     fontsize=11, fontweight='bold')
        ax4.set_xlabel('Sample Index')
        ax4.set_ylabel('Amplitude')
        ax4.grid(True, alpha=0.3)
        ax4.axhline(0, color='r', linestyle='--', alpha=0.5)
        
        # Plot 5: Spectrograms comparison
        ax5 = fig.add_subplot(gs[3, 0])
        # Original (first few seconds for comparison)
        sample_len = min(len(data), sr * 2)  # 2 seconds max
        f, t, Sxx = scipy_signal.spectrogram(data[:sample_len], sr, nperseg=256)
        ax5.pcolormesh(t, f, 10 * np.log10(Sxx + 1e-10), shading='gouraud', cmap='viridis')
        ax5.set_title('Spectrogram (Original - first 2s)', fontsize=11, fontweight='bold')
        ax5.set_ylabel('Frequency (Hz)')
        ax5.set_xlabel('Time (s)')
        ax5.set_ylim([0, min(sr/2, 8000)])
        
        ax6 = fig.add_subplot(gs[3, 1])
        # Resampled - approximate equivalent sample rate
        equiv_sr = sr * N_samples / len(data)
        if equiv_sr > 100:  # Only if meaningful
            f2, t2, Sxx2 = scipy_signal.spectrogram(resampled, equiv_sr, nperseg=min(64, N_samples//4))
            ax6.pcolormesh(t2, f2, 10 * np.log10(Sxx2 + 1e-10), shading='gouraud', cmap='viridis')
            ax6.set_ylim([0, min(equiv_sr/2, 8000)])
        else:
            ax6.text(0.5, 0.5, 'Sample rate too low\nfor spectrogram', 
                    ha='center', va='center', transform=ax6.transAxes)
        ax6.set_title('Spectrogram (Condensed)', fontsize=11, fontweight='bold')
        ax6.set_ylabel('Frequency (Hz)')
        ax6.set_xlabel('Time (normalized)')
        
        # Plot 6: Final LUT
        ax7 = fig.add_subplot(gs[4, :])
        ax7.plot(lut, 'b-', linewidth=1.5)
        ax7.axhline(2048, color='r', linestyle='--', label='Midpoint (2048)', linewidth=1.5)
        ax7.fill_between(range(len(lut)), lut, 2048, alpha=0.2)
        ax7.set_title(f'Final 12-bit LUT ({len(lut)} samples)', fontsize=11, fontweight='bold')
        ax7.set_xlabel('Sample Index')
        ax7.set_ylabel('Value (0-4095)')
        ax7.set_ylim([0, 4095])
        ax7.legend()
        ax7.grid(True, alpha=0.3)
        
        plt.suptitle(f'{os.path.basename(path)} - {len(data)/N_samples:.0f}x compression - {N_samples*4/1024:.2f}KB', 
                    fontsize=14, fontweight='bold')
        
        save_name = f"{os.path.splitext(os.path.basename(path))[0]}_{N_samples}_fullsignal.png"
        plt.savefig(save_name, dpi=150, bbox_inches='tight')
        print(f"  Plot saved: {save_name}")
        plt.show()
    
    return lut

# ============================================================================
# MAIN EXECUTION
# ============================================================================
if __name__ == '__main__':
    # Configuration
    N_SAMPLES = 1024  # Try 256, 512, 1024, 2048, or 4096
    
    # Strategy options:
    # 'energy' - Pure energy-adaptive (good for sustained sounds)
    # 'events' - Event-based (good for percussive/rhythmic)
    # 'mixed'  - Hybrid (RECOMMENDED - works well for everything)
    STRATEGY = 'mixed'
    
    file_paths = [
        r"C:\Users\rosto\OneDrive - University of Cape Town\Fourth Year\Sem 2\EEE3096S\EEE3096S Git Repo SMTROS022\EEE3096S SMTROS022 Pracs\Prac 4\Misc\Task 1\piano.wav",
        r"C:\Users\rosto\OneDrive - University of Cape Town\Fourth Year\Sem 2\EEE3096S\EEE3096S Git Repo SMTROS022\EEE3096S SMTROS022 Pracs\Prac 4\Misc\Task 1\guitar.wav",
        r"C:\Users\rosto\OneDrive - University of Cape Town\Fourth Year\Sem 2\EEE3096S\EEE3096S Git Repo SMTROS022\EEE3096S SMTROS022 Pracs\Prac 4\Misc\Task 1\drum.wav"
    ]
    
    print("\n" + "="*70)
    print(f"FULL SIGNAL COMPRESSION")
    print(f"  Strategy: {STRATEGY}")
    print(f"  Target: {N_SAMPLES} samples per file")
    print(f"  Memory per file: {N_SAMPLES * 4 / 1024:.2f}KB")
    print(f"  Total for 3 files: {3 * N_SAMPLES * 4 / 1024:.2f}KB")
    print("="*70)
    
    # Process all files
    results = {}
    for filepath in file_paths:
        if os.path.exists(filepath):
            lut = process_full_audio(filepath, N_SAMPLES, strategy=STRATEGY, show_plots=True)
            results[filepath] = lut
        else:
            print(f"\n⚠ WARNING: File not found: {filepath}")
    
    # Export
    print("\n" + "="*70)
    print("C ARRAY OUTPUT:")
    print("="*70)
    print()
    
    array_define = f"NS_AUDIO"
    print(f"#define {array_define}  {N_SAMPLES}\n")
    
    for filepath, lut in results.items():
        print(export_c_array(filepath, lut, array_define))
    
    print("="*70)
    print("✓ COMPLETE - Entire signal condensed while preserving key audio features")
    print("="*70)