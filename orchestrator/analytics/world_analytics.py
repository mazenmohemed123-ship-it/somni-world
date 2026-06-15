"""
WorldAnalytics — query simulation history using DuckDB + Polars.
Connects to the rolling event log from StateSyncBridge.
All analytics are READ-ONLY — never touches the simulation kernel.
"""
from __future__ import annotations

import json
from pathlib import Path
from typing import Dict, List, Optional, Any


class WorldAnalytics:
    """
    Analytics engine for SOMNI simulation data.
    Uses DuckDB for SQL queries and Polars for DataFrame operations.
    Both are optional — gracefully degrades to pure-Python fallback.
    """

    def __init__(self, events: Optional[List[Dict]] = None,
                 snapshot_dir: str = "worlds/"):
        self._events = events or []
        self._snapshot_dir = Path(snapshot_dir)
        self._db = None
        self._df = None
        self._setup_backends()

    def _setup_backends(self) -> None:
        if not self._events:
            return
        try:
            import duckdb
            self._db = duckdb.connect(":memory:")
            self._db.execute("""
                CREATE TABLE events (
                    tick     BIGINT,
                    type     VARCHAR,
                    npc_id   INTEGER,
                    faction  INTEGER,
                    cause    VARCHAR,
                    region   INTEGER,
                    amount   DOUBLE
                )
            """)
            for e in self._events:
                self._db.execute(
                    "INSERT INTO events VALUES (?, ?, ?, ?, ?, ?, ?)",
                    [
                        e.get("tick", 0),
                        e.get("type", ""),
                        e.get("npc_id"),
                        e.get("faction_a") or e.get("attacker"),
                        e.get("cause"),
                        e.get("region"),
                        e.get("amount"),
                    ]
                )
        except ImportError:
            pass

        try:
            import polars as pl
            self._df = pl.DataFrame(self._events) if self._events else None
        except ImportError:
            pass

    # ------------------------------------------------------------------
    # SQL queries (DuckDB)
    # ------------------------------------------------------------------

    def sql(self, query: str) -> Any:
        """Run arbitrary SQL against the events table."""
        if self._db is None:
            raise RuntimeError("pip install duckdb  to use SQL queries")
        return self._db.execute(query).fetchdf()

    def deaths_by_cause(self) -> Any:
        return self.sql("""
            SELECT cause, COUNT(*) as count
            FROM events
            WHERE type = 'npc_died'
            GROUP BY cause
            ORDER BY count DESC
        """)

    def wars_over_time(self) -> Any:
        return self.sql("""
            SELECT tick, COUNT(*) as wars_started
            FROM events
            WHERE type = 'faction_war_start'
            GROUP BY tick
            ORDER BY tick
        """)

    def faction_kill_count(self) -> Any:
        return self.sql("""
            SELECT faction, COUNT(*) as kills
            FROM events
            WHERE type = 'npc_died' AND cause = 'combat'
            GROUP BY faction
            ORDER BY kills DESC
        """)

    def event_timeline(self, event_type: str, limit: int = 100) -> Any:
        return self.sql(f"""
            SELECT * FROM events
            WHERE type = '{event_type}'
            ORDER BY tick
            LIMIT {limit}
        """)

    # ------------------------------------------------------------------
    # Polars DataFrames
    # ------------------------------------------------------------------

    def as_dataframe(self) -> Any:
        if self._df is None:
            raise RuntimeError("pip install polars  to use DataFrames")
        return self._df

    def death_rate_by_tick(self) -> Any:
        if self._df is None:
            raise RuntimeError("pip install polars  to use DataFrames")
        import polars as pl
        return (
            self._df
            .filter(pl.col("type") == "npc_died")
            .group_by("tick")
            .agg(pl.len().alias("deaths"))
            .sort("tick")
        )

    def faction_network(self) -> Any:
        """
        Build a NetworkX faction relationship graph from war events.
        Returns a DiGraph where edge weight = number of wars.
        """
        try:
            import networkx as nx
        except ImportError:
            raise RuntimeError("pip install networkx  to use faction network analysis")

        G = nx.DiGraph()
        for e in self._events:
            if e.get("type") == "faction_war_start":
                a = e.get("attacker", 0)
                d = e.get("defender", 0)
                if G.has_edge(a, d):
                    G[a][d]["wars"] += 1
                else:
                    G.add_edge(a, d, wars=1, relation="hostile")
        return G

    # ------------------------------------------------------------------
    # Pure-Python fallback analytics (always available)
    # ------------------------------------------------------------------

    def summary(self) -> Dict[str, Any]:
        """Always-available summary, no extra deps."""
        deaths = [e for e in self._events if e.get("type") == "npc_died"]
        wars   = [e for e in self._events if e.get("type") == "faction_war_start"]
        trades = [e for e in self._events if e.get("type") == "trade_completed"]

        causes: Dict[str, int] = {}
        for d in deaths:
            c = d.get("cause", "unknown")
            causes[c] = causes.get(c, 0) + 1

        return {
            "total_events":    len(self._events),
            "total_deaths":    len(deaths),
            "total_wars":      len(wars),
            "total_trades":    len(trades),
            "deaths_by_cause": causes,
        }

    def print_report(self) -> None:
        s = self.summary()
        print("\n=== SOMNI Analytics Report ===")
        print(f"  Total events logged : {s['total_events']}")
        print(f"  NPC deaths          : {s['total_deaths']}")
        print(f"  Wars started        : {s['total_wars']}")
        print(f"  Trades completed    : {s['total_trades']}")
        print(f"  Death causes        : {s['deaths_by_cause']}")
