import numpy as np
import matplotlib.pyplot as plt

def generate_waveform_luts(N=1024):
    """
    Generate sine, sawtooth, and triangle wave LUTs
    N: Number of samples (1024)
    Range: 0 to 4095 (12-bit DAC)
    """
    
    # Generate sample indices
    t = np.arange(N)
    
    # ========================================
    # 1. SINE WAVE
    # ========================================
    # Formula: 2048 + 2047 * sin(2π * t / N)
    sine_wave = 2048 + 2047 * np.sin(2 * np.pi * t / N)
    sine_lut = np.round(sine_wave).astype(np.uint32)
    sine_lut = np.clip(sine_lut, 0, 4095)
    
    # ========================================
    # 2. SAWTOOTH WAVE
    # ========================================
    # Formula: Linear ramp from 0 to 4095
    sawtooth_wave = (4096 / N) * t
    sawtooth_lut = np.round(sawtooth_wave).astype(np.uint32)
    sawtooth_lut = np.clip(sawtooth_lut, 0, 4095)
    
    # ========================================
    # 3. TRIANGLE WAVE
    # ========================================
    # Ramp up for first half, ramp down for second half
    triangle_wave = np.zeros(N)
    half = N // 2
    
    # Rising edge (0 to 4095)
    triangle_wave[:half] = (4096 / half) * np.arange(half)
    
    # Falling edge (4095 to 0)
    triangle_wave[half:] = 4095 - (4096 / half) * np.arange(N - half)
    
    triangle_lut = np.round(triangle_wave).astype(np.uint32)
    triangle_lut = np.clip(triangle_lut, 0, 4095)
    
    return sine_lut, sawtooth_lut, triangle_lut

def export_c_array(name, arr, define_name="NS_WAVEFORM"):
    """Export LUT as C array"""
    out = f"// {name} Wave LUT: {len(arr)} samples, {len(arr)*4:,} bytes ({len(arr)*4/1024:.2f}KB)\n"
    out += f"uint32_t {name}_LUT[{define_name}] = {{\n"
    
    for i in range(0, len(arr), 16):
        chunk = ', '.join(f"{int(x):4d}" for x in arr[i:i+16])
        if i + 16 < len(arr):
            out += f"    {chunk},\n"
        else:
            out += f"    {chunk}\n"
    
    out += "};\n"
    return out

def plot_waveforms(sine_lut, sawtooth_lut, triangle_lut, N):
    """Create visualization of all three waveforms"""
    
    fig, axes = plt.subplots(3, 2, figsize=(16, 10))
    fig.suptitle(f'Waveform LUTs ({N} samples each)', fontsize=16, fontweight='bold')
    
    waveforms = [
        (sine_lut, 'Sine', 'blue'),
        (sawtooth_lut, 'Sawtooth', 'green'),
        (triangle_lut, 'Triangle', 'red')
    ]
    
    for idx, (lut, name, color) in enumerate(waveforms):
        # Time domain plot
        ax_time = axes[idx, 0]
        ax_time.plot(lut, color=color, linewidth=1.5, alpha=0.8)
        ax_time.axhline(2048, color='black', linestyle='--', alpha=0.5, label='Midpoint (2048)')
        ax_time.set_title(f'{name} Wave - Time Domain', fontsize=12, fontweight='bold')
        ax_time.set_xlabel('Sample Index')
        ax_time.set_ylabel('Value (0-4095)')
        ax_time.set_ylim([0, 4095])
        ax_time.grid(True, alpha=0.3)
        ax_time.legend()
        
        # Add statistics text
        stats_text = f"Min: {np.min(lut)}\nMax: {np.max(lut)}\nMean: {np.mean(lut):.1f}\nStd: {np.std(lut):.1f}"
        ax_time.text(0.02, 0.98, stats_text, transform=ax_time.transAxes,
                    fontsize=9, verticalalignment='top',
                    bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5))
        
        # Frequency domain plot
        ax_freq = axes[idx, 1]
        
        # Convert to centered signal for FFT
        centered = lut.astype(float) - 2048
        fft_result = np.fft.rfft(centered)
        freqs = np.fft.rfftfreq(N)
        magnitude = np.abs(fft_result)
        
        # Plot in dB scale
        magnitude_db = 20 * np.log10(magnitude + 1e-10)
        ax_freq.plot(freqs[:len(freqs)//2], magnitude_db[:len(freqs)//2], 
                    color=color, linewidth=1.5)
        ax_freq.set_title(f'{name} Wave - Frequency Spectrum', fontsize=12, fontweight='bold')
        ax_freq.set_xlabel('Normalized Frequency')
        ax_freq.set_ylabel('Magnitude (dB)')
        ax_freq.grid(True, alpha=0.3)
        
        # Highlight fundamental and harmonics for reference
        if idx == 0:  # Sine - only fundamental
            ax_freq.axvline(1/N, color='red', linestyle='--', alpha=0.5, label='Fundamental')
        ax_freq.legend()
    
    plt.tight_layout()
    plt.savefig(f'waveforms_{N}_samples.png', dpi=150, bbox_inches='tight')
    print(f"  Plot saved: waveforms_{N}_samples.png")
    plt.show()

def print_statistics(sine_lut, sawtooth_lut, triangle_lut, N):
    """Print detailed statistics about the waveforms"""
    print("\n" + "="*70)
    print("WAVEFORM STATISTICS")
    print("="*70)
    
    waveforms = [
        ("Sine", sine_lut),
        ("Sawtooth", sawtooth_lut),
        ("Triangle", triangle_lut)
    ]
    
    for name, lut in waveforms:
        print(f"\n{name} Wave:")
        print(f"  Samples: {len(lut)}")
        print(f"  Memory: {len(lut) * 4:,} bytes ({len(lut) * 4 / 1024:.2f} KB)")
        print(f"  Range: [{np.min(lut)}, {np.max(lut)}]")
        print(f"  Mean: {np.mean(lut):.2f}")
        print(f"  Std Dev: {np.std(lut):.2f}")
        print(f"  DC Offset: {np.mean(lut) - 2048:.2f} (should be ~0 for centered)")

# ============================================================================
# MAIN EXECUTION
# ============================================================================
if __name__ == '__main__':
    N = 1024  # Number of samples
    
    print("\n" + "="*70)
    print(f"GENERATING WAVEFORM LUTs WITH {N} SAMPLES")
    print("="*70)
    
    # Generate waveforms
    sine_lut, sawtooth_lut, triangle_lut = generate_waveform_luts(N)
    
    # Print statistics
    print_statistics(sine_lut, sawtooth_lut, triangle_lut, N)
    
    # Create visualization
    print("\nGenerating plots...")
    plot_waveforms(sine_lut, sawtooth_lut, triangle_lut, N)
    
    # Export C arrays
    print("\n" + "="*70)
    print("C ARRAY OUTPUT (copy to main.c):")
    print("="*70)
    print()
    
    print(f"// Add to your defines section:")
    print(f"#define NS_WAVEFORM_1K  {N}\n")
    
    print(export_c_array("Sin", sine_lut, "NS_WAVEFORM_1K"))
    print(export_c_array("Saw", sawtooth_lut, "NS_WAVEFORM_1K"))
    print(export_c_array("Triangle", triangle_lut, "NS_WAVEFORM_1K"))
    
    print("="*70)
    print(f"✓ DONE! Generated 3 waveforms with {N} samples each")
    print(f"  Total memory: {3 * N * 4 / 1024:.2f} KB")
    print("="*70)