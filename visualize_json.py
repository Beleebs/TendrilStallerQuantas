import json
import sys
import numpy as np
import matplotlib.pyplot as plt
from pathlib import Path
import networkx as nx
from collections import defaultdict


def usage():
    print("Usage: python visualize_json.py <json_path> <count>")
    print("       python visualize_json.py <json_path> <node_id> [node_id] ...")
    print("Example: python visualize_json.py experiments/.../file.json 10")
    print("Example: python visualize_json.py experiments/.../file.json 0 3 5 7")


def parse_json_snapshots(json_path):
    with open(json_path, 'r') as f:
        data = json.load(f)

    tests = data.get('tests')
    if not tests or not isinstance(tests, list):
        raise ValueError('JSON does not contain a top-level "tests" list')

    first_test = tests[0]
    # Collect numeric round keys in sorted order
    round_keys = [k for k in first_test.keys() if k.isdigit()]
    round_keys.sort(key=lambda k: int(k))

    snapshots = []
    for rk in round_keys:
        entries = first_test[rk]
        if not isinstance(entries, list):
            continue

        snapshot = {}
        for entry in entries:
            if not isinstance(entry, dict):
                continue
            for node_str, blocks_str in entry.items():
                try:
                    node_id = int(node_str)
                except ValueError:
                    continue
                blocks = [b.strip() for b in blocks_str.split(',') if b.strip() and b.strip().lower() != 'tip']
                snapshot[node_id] = blocks

        # Only append snapshots that have at least one node
        if snapshot:
            snapshots.append(snapshot)

    return snapshots


def main(argv):
    if len(argv) < 3:
        usage()
        sys.exit(1)

    json_path = argv[1]
    nodes_args = [int(x) for x in argv[2:]]

    if len(nodes_args) == 1:
        num_nodes = nodes_args[0]
        target_node_ids = list(range(num_nodes))
    else:
        target_node_ids = nodes_args

    snapshots = parse_json_snapshots(json_path)

    out_dir = Path('visualizations')
    out_dir.mkdir(parents=True, exist_ok=True)

    for target_node_id in target_node_ids:
        G = nx.DiGraph()

        for snapshot_idx, snapshot in enumerate(snapshots):
            if target_node_id in snapshot:
                blocks = snapshot[target_node_id]
                for block in blocks:
                    G.add_node(block)
                for i in range(len(blocks) - 1):
                    parent = blocks[i]
                    child = blocks[i + 1]
                    if not G.has_edge(parent, child):
                        G.add_edge(parent, child, weight=snapshot_idx)

        # Visualization
        plt.figure(figsize=(16, 12))

        pos = {}
        layers = defaultdict(list)

        def get_depth(node, memo={}):
            if node in memo:
                return memo[node]
            parents = list(G.predecessors(node))
            if not parents:
                depth = 0
            else:
                depth = 1 + max(get_depth(p, memo) for p in parents)
            memo[node] = depth
            return depth

        depths = {node: get_depth(node) for node in G.nodes()}
        max_depth = max(depths.values()) if depths else 0

        for node, depth in depths.items():
            layers[depth].append(node)

        for depth, nodes in layers.items():
            x = depth
            y_positions = np.linspace(-len(nodes), len(nodes), len(nodes))
            for i, node in enumerate(sorted(nodes)):
                pos[node] = (x, y_positions[i])

        nx.draw_networkx_nodes(G, pos, node_color='lightblue', node_size=1500, edgecolors='black', linewidths=2)
        nx.draw_networkx_edges(G, pos, edge_color='gray', arrows=True, arrowsize=20, arrowstyle='->', width=2)
        nx.draw_networkx_labels(G, pos, font_size=8)
        edge_labels = nx.get_edge_attributes(G, 'weight')
        nx.draw_networkx_edge_labels(G, pos, edge_labels=edge_labels, rotate=False, font_size=10, font_weight='bold')

        plt.title(f'Node {target_node_id} - Block Chain Simulation ({len(snapshots)} Rounds)', fontsize=16, fontweight='bold')
        plt.axis('off')
        plt.tight_layout()

        out_path = out_dir / f'block_chain_graph_node_{target_node_id}.png'
        plt.savefig(out_path, dpi=150, bbox_inches='tight')
        print(f"Graph saved as {out_path}")
        print(f"  Node ID: {target_node_id}")
        print(f"  Total blocks: {len(G.nodes())}")
        print(f"  Total connections: {len(G.edges())}")
        print(f"  Graph depth: {max_depth}")
        print()
        plt.close()

    print('Done!')


if __name__ == '__main__':
    main(sys.argv)
