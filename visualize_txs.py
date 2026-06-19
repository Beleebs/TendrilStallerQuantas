import json
import sys
import numpy as np
import matplotlib.pyplot as plt
from pprint import pprint
from pathlib import Path



def parse_json_txs(json_path):
    with open(json_path, 'r') as f:
        data = json.load(f)

    tests = data.get('tests')
    if not tests or not isinstance(tests, list):
        raise ValueError('JSON does not contain a top-level "tests" list')
    
    for test in tests:
        txs = test.get("UTXOsPerRound")
        return txs

def main(argv):
    if len(argv) < 2:
        print("usage: python visualize_txs.py [path to json]")
        sys.exit(1)

    json_path = argv[1]
    tx_list = parse_json_txs(json_path)
    if not isinstance(tx_list, list):
        print("Parsed tx_list is not a list")
        sys.exit(1)

    # x values: rounds starting at 0
    tx_list.insert(0, 0)
    print(tx_list)
    x = list(range(0, len(tx_list)))
    y = tx_list

    plt.figure(figsize=(8, 4))
    plt.plot(x, y, marker='o', linestyle='-')
    plt.xlabel('Round')
    plt.ylabel('Transactions')
    plt.title('Total Unique Transactions in All Mempools')
    plt.grid(True)
    plt.tight_layout()
    
    out_dir = Path('visualizations')
    out_path = out_dir / f'txs_graph.png'
    plt.savefig(out_path, dpi=150, bbox_inches='tight')
    print("done!")
    plt.close()


if __name__ == '__main__':
    main(sys.argv)