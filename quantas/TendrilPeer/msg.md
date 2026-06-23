## Template for making messages:

```json
[
    // HEADER
    {
        "type": 0, 
        "content": {
            "block_id": 0,
            "prev_id":-1,
            "round_mined":1,
            "miner_id":-1,
        }
    },
    // GET_DATA
    {
        "type": 1,
        "content": {
            "block_id_requested":0
        }
    },
    // CMP_BLOCK
    {
        "type": 2,
        "content": {
            "block_id":0,
            "prev_id":-1,
            "round_mined":0,
            "miner_id":-1,
            "tx_ids":[
                0,
                1, 
                2, 
                3, 
                4, 
                5
            ]
        }
    },
    // GET_BLOCK_TXN
    {
        "type": 3,
        "content": {
            "block_id_requested":0
        }
    },
    // BLOCK_TXN
    {
        "type": 4,
        "content": {
            "txs":[
                {
                    "type": 5,
                    "content": {
                        "tx_id":1,
                        "round_created":-1,
                        "source_id":-1,
                        "receiver_id":-2
                    }
                },
                {
                    "type": 5,
                    "content": {
                        "tx_id":2,
                        "round_created":-1,
                        "source_id":-1,
                        "receiver_id":-2
                    }
                },
                {
                    "type": 5,
                    "content": {
                        "tx_id":3,
                        "round_created":-1,
                        "source_id":-1,
                        "receiver_id":-2
                    }
                },
                {
                    "type": 5,
                    "content": {
                        "tx_id":4,
                        "round_created":-1,
                        "source_id":-1,
                        "receiver_id":-2
                    }
                },
                {
                    "type": 5,
                    "content": {
                        "tx_id":5,
                        "round_created":-1,
                        "source_id":-1,
                        "receiver_id":-2
                    }
                },
            ]
        }
    },
    // TXN
    {
        "type": 5,
        "content": {
            "tx_id":0,
            "round_created":-1,
            "source_id":-1,
            "receiver_id":-2
        }
    },
]
```