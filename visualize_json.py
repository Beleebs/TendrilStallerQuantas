import re
import sys
import numpy as np
import matplotlib.pyplot as plt
import os
from pathlib import Path
import networkx as nx
from collections import defaultdict

# Get node IDs from command line arguments
if len(sys.argv) < 2:
    print("Usage: python visualize_json.py <count>")
    print("       python visualize_json.py <node_id> [node_id] [node_id] ...")
    print("Example: python visualize_json.py 10")
    print("Example: python visualize_json.py 0 3 5 7")
    sys.exit(1)

# If only one argument, treat it as count of nodes (0 to count-1)
if len(sys.argv) == 2:
    num_nodes = int(sys.argv[1])
    target_node_ids = list(range(num_nodes))
else:
    # If multiple arguments, treat each as a specific node ID
    target_node_ids = [int(arg) for arg in sys.argv[1:]]

# Parse the file
snapshots = []
current_snapshot = {}

with open('out.txt', 'r') as f:
    for line in f:
        match = re.match(r"(\d+)'s local blockchain: (.*)", line)
        if match:
            node_id = int(match.group(1))
            blocks_str = match.group(2)
            blocks = [b.strip() for b in blocks_str.split(',')]
            blocks = [b for b in blocks if b != 'tip']  # Filter out 'tip'
            current_snapshot[node_id] = blocks
            
            # Once we have all nodes, save the snapshot
            if len(current_snapshot) == 10:  # Assuming 10 nodes (0-9)
                snapshots.append(current_snapshot.copy())
                current_snapshot = {}

# Create output directory
out_dir = Path('visualizations')
out_dir.mkdir(parents=True, exist_ok=True)

# Generate graph for each target node
for target_node_id in target_node_ids:
    # Build directed graph for specific node
    G = nx.DiGraph()

    # Track edges and nodes for the target node only
    for snapshot_idx, snapshot in enumerate(snapshots):
        if target_node_id in snapshot:
            blocks = snapshot[target_node_id]
            
            # Add nodes to graph
            for block in blocks:
                G.add_node(block)
            
            # Add edges (parent -> child blocks)
            for i in range(len(blocks) - 1):
                parent = blocks[i]
                child = blocks[i + 1]
                if not G.has_edge(parent, child):
                    G.add_edge(parent, child, weight=snapshot_idx)

    # Create the visualization
    plt.figure(figsize=(16, 12))

    pos = {}
    layers = defaultdict(list)

    # Calculate depth of each node (distance from genesis)
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

    # Group nodes by depth
    for node, depth in depths.items():
        layers[depth].append(node)

    # Position nodes in layers
    for depth, nodes in layers.items():
        x = depth
        y_positions = np.linspace(-len(nodes), len(nodes), len(nodes))
        for i, node in enumerate(sorted(nodes)):
            pos[node] = (x, y_positions[i])

    # Draw the graph
    nx.draw_networkx_nodes(G, pos, node_color='lightblue', node_size=1500, edgecolors='black', linewidths=2)
    nx.draw_networkx_edges(G, pos, edge_color='gray', arrows=True, arrowsize=20, arrowstyle='->', width=2)
    nx.draw_networkx_labels(G, pos, font_size=8)
    edge_labels = nx.get_edge_attributes(G, "weight")
    nx.draw_networkx_edge_labels(G, pos, edge_labels=edge_labels, rotate=False, font_size=10, font_weight='bold')

    plt.title(f'Node {target_node_id} - Block Chain Propagation with Forks', fontsize=16, fontweight='bold')
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

print("Done!")