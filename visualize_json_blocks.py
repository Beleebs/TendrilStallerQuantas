#!/usr/bin/env python3
import argparse
import json
import os
from collections import defaultdict
from typing import Any, Dict, List, Optional, Tuple

import matplotlib.pyplot as plt

try:
    import networkx as nx
except ImportError:
    nx = None


def load_json(path: str) -> Any:
    with open(path, 'r', encoding='utf-8') as f:
        return json.load(f)


def is_block_object(obj: Any) -> bool:
    return isinstance(obj, dict) and obj.get('type') == 'block' and isinstance(obj.get('contents'), dict)


def find_blocks(obj: Any, path: str = '') -> List[Dict[str, Any]]:
    found = []
    if is_block_object(obj):
        found.append(obj['contents'])
    elif isinstance(obj, dict):
        for key, value in obj.items():
            found.extend(find_blocks(value, f"{path}.{key}" if path else key))
    elif isinstance(obj, list):
        for idx, item in enumerate(obj):
            found.extend(find_blocks(item, f"{path}[{idx}]") if path else find_blocks(item, f"[{idx}]"))
    return found


def parse_transaction(obj: Dict[str, Any]) -> Dict[str, Any]:
    contents = obj.get('contents', {}) if isinstance(obj, dict) else {}
    return {
        'txID': contents.get('txID'),
        'sender': contents.get('ID_sender'),
        'receiver': contents.get('ID_receiver'),
        'roundSent': contents.get('roundSent'),
        'type': obj.get('type', 'tx'),
    }


def summarize_blocks(blocks: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
    results = []
    for block in blocks:
        txs = block.get('transactions', []) or []
        parsed_transactions = [parse_transaction(tx) for tx in txs if isinstance(tx, dict)]
        results.append({
            'blockID': block.get('blockID'),
            'height': block.get('height'),
            'roundMined': block.get('roundMined'),
            'prevID': block.get('prevID'),
            'numberOfTransactions': block.get('numberOfTransactions', len(parsed_transactions)),
            'transactions': parsed_transactions,
        })
    return results


def collect_transaction_events(blocks: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
    events = []
    for block in blocks:
        block_id = block.get('blockID')
        for tx in block.get('transactions', []) or []:
            parsed = parse_transaction(tx)
            parsed['blockID'] = block_id
            events.append(parsed)
    return events


def plot_block_timeline(blocks: List[Dict[str, Any]], output_path: str) -> None:
    heights = [b['height'] for b in blocks if b['height'] is not None]
    mined = [b['roundMined'] for b in blocks if b['roundMined'] is not None]
    labels = [str(b['blockID']) for b in blocks if b['height'] is not None and b['roundMined'] is not None]

    if not heights or not mined:
        return

    plt.figure(figsize=(10, 5))
    plt.scatter(heights, mined, c='tab:blue', edgecolors='black')
    for i, label in enumerate(labels):
        if i % max(1, len(labels) // 20) == 0:
            plt.text(heights[i], mined[i], label, fontsize=7, alpha=0.7)
    plt.title('Blocks: Height vs. Round Mined')
    plt.xlabel('Block Height')
    plt.ylabel('Round Mined')
    plt.grid(alpha=0.3)
    plt.tight_layout()
    plt.savefig(output_path)
    plt.close()


def plot_transactions_timeline(transactions: List[Dict[str, Any]], output_path: str) -> None:
    events = [tx for tx in transactions if tx.get('roundSent') is not None]
    if not events:
        return

    x = [tx['roundSent'] for tx in events]
    y = [tx['blockID'] for tx in events]
    colors = []
    for tx in events:
        if tx.get('sender') == tx.get('receiver'):
            colors.append('tab:purple')
        elif tx.get('sender') is None or tx.get('receiver') is None:
            colors.append('tab:gray')
        else:
            colors.append('tab:green')

    plt.figure(figsize=(10, 5))
    plt.scatter(x, y, c=colors, alpha=0.7, edgecolors='black')
    plt.title('Transactions by Round Sent')
    plt.xlabel('Round Sent')
    plt.ylabel('Block ID')
    plt.grid(alpha=0.3)
    plt.tight_layout()
    plt.savefig(output_path)
    plt.close()


def plot_transaction_counts(blocks: List[Dict[str, Any]], output_path: str) -> None:
    valid = [b for b in blocks if b['height'] is not None and b['numberOfTransactions'] is not None]
    if not valid:
        return

    valid.sort(key=lambda b: (b['height'], b['blockID']))
    heights = [b['height'] for b in valid]
    counts = [b['numberOfTransactions'] for b in valid]

    plt.figure(figsize=(10, 5))
    plt.bar(heights, counts, color='tab:orange', edgecolor='black')
    plt.title('Transaction Count per Block Height')
    plt.xlabel('Block Height')
    plt.ylabel('Number of Transactions')
    plt.tight_layout()
    plt.savefig(output_path)
    plt.close()


def plot_block_graph(blocks: List[Dict[str, Any]], output_path: str) -> None:
    if nx is None:
        print('networkx not installed; skipping block graph plot.')
        return

    graph = nx.DiGraph()
    for block in blocks:
        block_id = block.get('blockID')
        if block_id is None:
            continue
        graph.add_node(block_id)
        if block.get('prevID') is not None:
            graph.add_edge(block['prevID'], block_id)

    if graph.number_of_nodes() == 0:
        return

    plt.figure(figsize=(12, 8))
    pos = nx.spring_layout(graph, seed=42)
    nx.draw(graph, pos, with_labels=True, node_color='skyblue', edge_color='gray', node_size=600)
    plt.title('Block Parent Graph')
    plt.tight_layout()
    plt.savefig(output_path)
    plt.close()


def print_summary(blocks: List[Dict[str, Any]], transactions: List[Dict[str, Any]]) -> None:
    print('=== JSON Block/Transaction Summary ===')
    print(f'Total blocks found: {len(blocks)}')
    print(f'Total transactions found: {len(transactions)}')
    heights = [b['height'] for b in blocks if b['height'] is not None]
    rounds = [b['roundMined'] for b in blocks if b['roundMined'] is not None]
    if heights:
        print(f'Block height range: {min(heights)} .. {max(heights)}')
    if rounds:
        print(f'Round mined range: {min(rounds)} .. {max(rounds)}')

    counts = defaultdict(int)
    for tx in transactions:
        key = (tx.get('sender'), tx.get('receiver'))
        counts[key] += 1
    if counts:
        print('Top 5 sender->receiver pairs:')
        for pair, count in sorted(counts.items(), key=lambda kv: kv[1], reverse=True)[:5]:
            print(f'  {pair}: {count}')


def main() -> None:
    parser = argparse.ArgumentParser(description='Visualize JSON block and transaction objects.')
    parser.add_argument('json_file', help='Path to the JSON file to visualize')
    parser.add_argument('--output-dir', default='visualization_output', help='Directory to write PNG outputs')
    parser.add_argument('--no-show', action='store_true', help='Do not display plots interactively')
    parser.add_argument('--graph', action='store_true', help='Create block parent graph if networkx is installed')
    args = parser.parse_args()

    data = load_json(args.json_file)
    blocks = summarize_blocks(find_blocks(data))
    transactions = collect_transaction_events(find_blocks(data))

    if not blocks:
        print('No block objects were detected in the JSON file.')
        return

    os.makedirs(args.output_dir, exist_ok=True)
    print_summary(blocks, transactions)

    timelines_path = os.path.join(args.output_dir, 'block_height_vs_round.png')
    plot_block_timeline(blocks, timelines_path)
    print(f'Wrote {timelines_path}')

    counts_path = os.path.join(args.output_dir, 'block_transaction_counts.png')
    plot_transaction_counts(blocks, counts_path)
    print(f'Wrote {counts_path}')

    tx_path = os.path.join(args.output_dir, 'transaction_rounds.png')
    plot_transactions_timeline(transactions, tx_path)
    print(f'Wrote {tx_path}')

    if args.graph:
        graph_path = os.path.join(args.output_dir, 'block_parent_graph.png')
        plot_block_graph(blocks, graph_path)
        print(f'Wrote {graph_path}')

    if not args.no_show:
        try:
            plt.show()
        except Exception:
            pass


if __name__ == '__main__':
    main()
