import React, { useState, useEffect } from 'react';
import { TrendingUp, TrendingDown, Clock, Flame } from 'lucide-react';

// ============================================
// MOCK DATA - ganti pake supabase nanti
// import { getTokens, subscribeToTokens } from '../lib/supabase';
// ============================================

const MOCK_TOKENS = [
  { id: 1, name: 'LobstarAI', symbol: '$LOBS', image: '🦞', price: 0.0042, change_24h: 12.5, market_cap: 42000, volume_24h: 8900, created_at: '2m ago' },
  { id: 2, name: 'ClawPunk', symbol: '$CPNK', image: '🦀', price: 0.0089, change_24h: -5.2, market_cap: 89000, volume_24h: 15200, created_at: '8m ago' },
  { id: 3, name: 'ShellMind', symbol: '$SHEL', image: '🐚', price: 0.0156, change_24h: 34.8, market_cap: 156000, volume_24h: 32100, created_at: '15m ago' },
  { id: 4, name: 'DeepClaw', symbol: '$DEEP', image: '🪸', price: 0.0023, change_24h: 8.1, market_cap: 23000, volume_24h: 4500, created_at: '22m ago' },
  { id: 5, name: 'TidalBot', symbol: '$TIDE', image: '🌊', price: 0.0067, change_24h: -2.1, market_cap: 67000, volume_24h: 11800, created_at: '31m ago' },
  { id: 6, name: 'ReefAgent', symbol: '$REEF', image: '🐠', price: 0.0198, change_24h: 45.2, market_cap: 198000, volume_24h: 45000, created_at: '1h ago' },
];

export default function TokenFeed() {
  const [tokens, setTokens] = useState(MOCK_TOKENS);
  const [filter, setFilter] = useState('all'); // all | trending | new

  // ============================================
  // UNCOMMENT BUAT REAL DATA:
  // useEffect(() => {
  //   getTokens(20).then(setTokens);
  //   const sub = subscribeToTokens((payload) => {
  //     setTokens(prev => [payload.new, ...prev]);
  //   });
  //   return () => sub.unsubscribe();
  // }, []);
  // ============================================

  const filtered = filter === 'trending'
    ? [...tokens].sort((a, b) => b.change_24h - a.change_24h)
    : filter === 'new'
      ? [...tokens].sort((a, b) => a.id - b.id)
      : tokens;

  return (
    <section id="trade" className="max-w-7xl mx-auto px-4 py-20">
      {/* Header */}
      <div className="flex flex-col sm:flex-row sm:items-center justify-between gap-4 mb-8">
        <div>
          <h2 className="font-display text-2xl font-bold text-claw-light flex items-center gap-2">
            <Flame className="w-6 h-6 text-claw-orange" />
            Just Launched
          </h2>
          <p className="text-sm text-claw-muted mt-1">Latest agent-launched tokens on Solana</p>
        </div>

        {/* Filter tabs */}
        <div className="flex items-center gap-1 bg-claw-surface rounded-xl p-1">
          {[
            { key: 'all', label: 'All' },
            { key: 'trending', label: 'Trending' },
            { key: 'new', label: 'New' },
          ].map(({ key, label }) => (
            <button
              key={key}
              onClick={() => setFilter(key)}
              className={`px-4 py-2 text-sm rounded-lg transition-all ${
                filter === key
                  ? 'bg-claw-card text-claw-light font-medium'
                  : 'text-claw-muted hover:text-claw-text'
              }`}
            >
              {label}
            </button>
          ))}
        </div>
      </div>

      {/* Token grid */}
      <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-4">
        {filtered.map((token, i) => (
          <TokenCard key={token.id} token={token} index={i} />
        ))}
      </div>
    </section>
  );
}

function TokenCard({ token, index }) {
  const isPositive = token.change_24h >= 0;

  return (
    <div
      className="glass-card p-4 hover:border-claw-muted/30 transition-all duration-300 cursor-pointer group animate-slide-up"
      style={{ animationDelay: `${index * 80}ms` }}
    >
      <div className="flex items-start justify-between mb-3">
        {/* Token info */}
        <div className="flex items-center gap-3">
          <div className="w-12 h-12 rounded-xl bg-claw-surface flex items-center justify-center text-2xl group-hover:scale-110 transition-transform">
            {token.image}
          </div>
          <div>
            <h3 className="font-display font-semibold text-claw-light">{token.name}</h3>
            <span className="text-xs text-claw-muted font-mono">{token.symbol}</span>
          </div>
        </div>

        {/* Time */}
        <div className="flex items-center gap-1 text-xs text-claw-muted">
          <Clock className="w-3 h-3" />
          {token.created_at}
        </div>
      </div>

      {/* Price & Change */}
      <div className="flex items-end justify-between">
        <div>
          <div className="text-lg font-display font-bold text-claw-light">
            ${token.price.toFixed(4)}
          </div>
          <div className="text-xs text-claw-muted mt-0.5">
            MC ${(token.market_cap / 1000).toFixed(1)}K
          </div>
        </div>

        <div className={`flex items-center gap-1 px-2 py-1 rounded-lg text-sm font-medium ${
          isPositive
            ? 'bg-claw-green/10 text-claw-green'
            : 'bg-claw-red/10 text-claw-red'
        }`}>
          {isPositive ? <TrendingUp className="w-3.5 h-3.5" /> : <TrendingDown className="w-3.5 h-3.5" />}
          {isPositive ? '+' : ''}{token.change_24h}%
        </div>
      </div>

      {/* Mini progress bar */}
      <div className="mt-3 h-1 bg-claw-surface rounded-full overflow-hidden">
        <div
          className="h-full rounded-full bg-gradient-to-r from-claw-red to-claw-orange transition-all duration-1000"
          style={{ width: `${Math.min(100, (token.market_cap / 200000) * 100)}%` }}
        />
      </div>
    </div>
  );
}
