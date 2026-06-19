import json
import sys
import numpy as np
import matplotlib.pyplot as plt
from pprint import pprint
from pathlib import Path

def parse_metrics(json_path):
    """Parse delay, throughput, and mempool metrics from JSON output."""
    with open(json_path, 'r') as f:
        data = json.load(f)
    
    tests = data.get('tests')
    if not tests or not isinstance(tests, list):
        raise ValueError('JSON does not contain a top-level "tests" list')
    
    test = tests[0]  # Use first test
    
    metrics = {
        'tx_confirmation_delay': test.get('tx_confirmation_delay', []),
        'block_propagation_delay': test.get('block_propagation_delay', []),
        'txs_created_per_round': test.get('txs_created_per_round', []),
        'txs_confirmed_in_mined_block': test.get('txs_confirmed_in_mined_block', []),
        'txs_confirmed_in_accepted_block': test.get('txs_confirmed_in_accepted_block', []),
        'blocks_mined_per_round': test.get('blocks_mined_per_round', []),
        'mempool_size_per_round': test.get('mempool_size_per_round', []),
        'UTXOsPerRound': test.get('UTXOsPerRound', []),
    }
    return metrics

def compute_stats(data):
    """Compute mean, std, min, max, and percentiles."""
    if not data or len(data) == 0:
        return {}
    data = np.array(data)
    return {
        'mean': float(np.mean(data)),
        'std': float(np.std(data)),
        'min': float(np.min(data)),
        'max': float(np.max(data)),
        'p50': float(np.percentile(data, 50)),
        'p90': float(np.percentile(data, 90)),
        'p99': float(np.percentile(data, 99)),
    }

def plot_delay_histogram(delays, title, filename, out_dir):
    """Plot histogram of delays with statistics."""
    if not delays:
        return
    
    stats = compute_stats(delays)
    fig, ax = plt.subplots(figsize=(10, 5))
    
    ax.hist(delays, bins=30, edgecolor='black', alpha=0.7, color='steelblue')
    ax.axvline(stats['mean'], color='red', linestyle='--', linewidth=2, label=f"Mean: {stats['mean']:.2f}")
    ax.axvline(stats['p50'], color='orange', linestyle='--', linewidth=2, label=f"Median: {stats['p50']:.2f}")
    ax.axvline(stats['p90'], color='green', linestyle='--', linewidth=2, label=f"P90: {stats['p90']:.2f}")
    
    ax.set_xlabel('Delay (rounds)')
    ax.set_ylabel('Frequency')
    ax.set_title(title)
    ax.legend()
    ax.grid(True, alpha=0.3)
    
    # Add text box with statistics
    stats_text = f"Samples: {len(delays)}\nMin: {stats['min']:.1f}\nMax: {stats['max']:.1f}\nStd: {stats['std']:.2f}"
    ax.text(0.98, 0.97, stats_text, transform=ax.transAxes, verticalalignment='top',
            horizontalalignment='right', bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5))
    
    plt.tight_layout()
    out_path = out_dir / filename
    plt.savefig(out_path, dpi=150, bbox_inches='tight')
    plt.close()
    print(f"Saved: {out_path}")

def plot_delay_cdf(delays, title, filename, out_dir):
    """Plot CDF of delays."""
    if not delays:
        return
    
    sorted_delays = np.sort(delays)
    cdf = np.arange(1, len(sorted_delays) + 1) / len(sorted_delays)
    
    fig, ax = plt.subplots(figsize=(10, 5))
    ax.plot(sorted_delays, cdf, linewidth=2, color='darkblue')
    ax.set_xlabel('Delay (rounds)')
    ax.set_ylabel('Cumulative Probability')
    ax.set_title(title)
    ax.grid(True, alpha=0.3)
    
    # Mark percentiles
    p90_val = np.percentile(sorted_delays, 90)
    p99_val = np.percentile(sorted_delays, 99)
    ax.axvline(p90_val, color='green', linestyle='--', linewidth=1.5, alpha=0.7, label=f"P90: {p90_val:.1f}")
    ax.axvline(p99_val, color='red', linestyle='--', linewidth=1.5, alpha=0.7, label=f"P99: {p99_val:.1f}")
    ax.legend()
    
    plt.tight_layout()
    out_path = out_dir / filename
    plt.savefig(out_path, dpi=150, bbox_inches='tight')
    plt.close()
    print(f"Saved: {out_path}")

def plot_throughput_over_time(metrics, out_dir):
    """Plot transaction and block throughput over time."""
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(12, 8))
    
    # Transaction creation over time (per-event)
    tx_rounds = np.arange(len(metrics['txs_created_per_round']))
    ax1.plot(tx_rounds, metrics['txs_created_per_round'], marker='o', linestyle='-', 
             label='Txs Created (per event)', linewidth=2, markersize=4, alpha=0.8, color='steelblue')
    ax1.set_ylabel('Transactions Created')
    ax1.set_title('Transaction Creation Over Time')
    ax1.legend()
    ax1.grid(True, alpha=0.3)
    
    # Block throughput (per-block event)
    block_rounds = np.arange(len(metrics['blocks_mined_per_round']))
    ax2_twin = ax2.twinx()
    
    line1 = ax2.plot(block_rounds, metrics['blocks_mined_per_round'], marker='^', 
             linestyle='-', label='Blocks Mined', linewidth=2, markersize=5, color='darkgreen', alpha=0.8)
    line2 = ax2_twin.plot(block_rounds, metrics['txs_confirmed_in_mined_block'], marker='s', 
             linestyle='--', label='Txs Confirmed per Block', linewidth=2, markersize=5, color='darkred', alpha=0.8)
    
    ax2.set_xlabel('Block Event')
    ax2.set_ylabel('Blocks Mined', color='darkgreen')
    ax2_twin.set_ylabel('Transactions Confirmed', color='darkred')
    ax2.set_title('Block Mining & Transaction Confirmation')
    ax2.tick_params(axis='y', labelcolor='darkgreen')
    ax2_twin.tick_params(axis='y', labelcolor='darkred')
    ax2.grid(True, alpha=0.3)
    
    # Combined legend
    lines = line1 + line2
    labels = [l.get_label() for l in lines]
    ax2.legend(lines, labels, loc='upper left')
    
    plt.tight_layout()
    out_path = out_dir / 'throughput_over_time.png'
    plt.savefig(out_path, dpi=150, bbox_inches='tight')
    plt.close()
    print(f"Saved: {out_path}")

def plot_mempool_pressure(metrics, out_dir):
    """Plot mempool size and UTXO backlog over time."""
    mempool_rounds = np.arange(len(metrics['mempool_size_per_round']))
    utxo_rounds = np.arange(len(metrics['UTXOsPerRound']))
    
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(12, 8))
    
    # Mempool size
    ax1.plot(mempool_rounds, metrics['mempool_size_per_round'], marker='o', linestyle='-', 
             label='Mempool Size', linewidth=2, markersize=4, color='purple', alpha=0.8)
    ax1.set_ylabel('Pending Transactions')
    ax1.set_title('Mempool Pressure Over Time')
    ax1.grid(True, alpha=0.3)
    ax1.legend()
    
    # UTXO backlog
    ax2.plot(utxo_rounds, metrics['UTXOsPerRound'], marker='s', linestyle='-', 
             label='Unique UTXOs in All Mempools', linewidth=2, markersize=4, color='teal', alpha=0.8)
    ax2.set_xlabel('Round')
    ax2.set_ylabel('Unique Transactions')
    ax2.set_title('Network-wide UTXO Backlog')
    ax2.grid(True, alpha=0.3)
    ax2.legend()
    
    plt.tight_layout()
    out_path = out_dir / 'mempool_pressure.png'
    plt.savefig(out_path, dpi=150, bbox_inches='tight')
    plt.close()
    print(f"Saved: {out_path}")

def print_summary_stats(metrics):
    """Print summary statistics to console."""
    print("\n" + "="*60)
    print("THROUGHPUT & DELAY SUMMARY STATISTICS")
    print("="*60)
    
    if metrics['tx_confirmation_delay']:
        stats = compute_stats(metrics['tx_confirmation_delay'])
        print(f"\nTransaction Confirmation Delay (rounds):")
        print(f"  Mean:     {stats['mean']:.2f}")
        print(f"  Std Dev:  {stats['std']:.2f}")
        print(f"  Min:      {stats['min']:.1f}")
        print(f"  Max:      {stats['max']:.1f}")
        print(f"  Median:   {stats['p50']:.1f}")
        print(f"  P90:      {stats['p90']:.1f}")
        print(f"  P99:      {stats['p99']:.1f}")
        print(f"  Samples:  {len(metrics['tx_confirmation_delay'])}")
    
    if metrics['block_propagation_delay']:
        stats = compute_stats(metrics['block_propagation_delay'])
        print(f"\nBlock Propagation Delay (rounds):")
        print(f"  Mean:     {stats['mean']:.2f}")
        print(f"  Std Dev:  {stats['std']:.2f}")
        print(f"  Min:      {stats['min']:.1f}")
        print(f"  Max:      {stats['max']:.1f}")
        print(f"  Median:   {stats['p50']:.1f}")
        print(f"  P90:      {stats['p90']:.1f}")
        print(f"  P99:      {stats['p99']:.1f}")
        print(f"  Samples:  {len(metrics['block_propagation_delay'])}")
    
    if metrics['txs_created_per_round']:
        total_created = sum(metrics['txs_created_per_round'])
        print(f"\nTransaction Creation:")
        print(f"  Total created:  {total_created}")
        print(f"  Per round avg:  {total_created / len(metrics['txs_created_per_round']):.2f}")
    
    if metrics['blocks_mined_per_round']:
        total_blocks = sum(metrics['blocks_mined_per_round'])
        print(f"\nBlock Mining:")
        print(f"  Total mined:    {total_blocks}")
        print(f"  Per round avg:  {total_blocks / len(metrics['blocks_mined_per_round']):.2f}")
    
    if metrics['mempool_size_per_round']:
        stats = compute_stats(metrics['mempool_size_per_round'])
        print(f"\nMempool Size:")
        print(f"  Mean:     {stats['mean']:.1f}")
        print(f"  Max:      {stats['max']:.1f}")
        print(f"  Min:      {stats['min']:.1f}")
    
    print("\n" + "="*60 + "\n")

def main(argv):
    if len(argv) < 2:
        print("usage: python visualize_delay.py [path to json]")
        sys.exit(1)

    json_path = argv[1]
    
    try:
        metrics = parse_metrics(json_path)
        print(f"Loaded metrics from: {json_path}")
    except Exception as e:
        print(f"Error loading JSON: {e}")
        sys.exit(1)
    
    # Create output directory
    out_dir = Path('visualizations')
    out_dir.mkdir(exist_ok=True)
    
    # Print summary statistics
    print_summary_stats(metrics)
    
    # Generate visualizations
    print("Generating graphs...")
    
    plot_delay_histogram(metrics['tx_confirmation_delay'], 
                        'Transaction Confirmation Delay Distribution', 
                        'tx_confirmation_delay_histogram.png', out_dir)
    
    plot_delay_cdf(metrics['tx_confirmation_delay'],
                   'Transaction Confirmation Delay CDF',
                   'tx_confirmation_delay_cdf.png', out_dir)
    
    plot_delay_histogram(metrics['block_propagation_delay'],
                        'Block Propagation Delay Distribution',
                        'block_propagation_delay_histogram.png', out_dir)
    
    plot_delay_cdf(metrics['block_propagation_delay'],
                   'Block Propagation Delay CDF',
                   'block_propagation_delay_cdf.png', out_dir)
    
    plot_throughput_over_time(metrics, out_dir)
    plot_mempool_pressure(metrics, out_dir)
    
    print(f"\nAll visualizations saved to: {out_dir}")


if __name__ == "__main__":
    main(sys.argv)

