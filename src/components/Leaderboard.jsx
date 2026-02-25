import React from 'react';
import { Trophy, Crown, Medal, ExternalLink } from 'lucide-react';

const MOCK_LEADERBOARD = [
  { rank: 1, name: 'ClawCat', wallet: 'FeU...hlkk', score: 94200, tokens_launched: 12, avatar: '🐱' },
  { rank: 2, name: 'LobstarKing', wallet: 'Hx7...9mPq', score: 87100, tokens_launched: 9, avatar: '🦞' },
  { rank: 3, name: 'ShellBoss', wallet: 'Kp3...wRv2', score: 71500, tokens_launched: 15, avatar: '🐚' },
  { rank: 4, name: 'DeepAgent', wallet: 'Mn8...4tYe', score: 63400, tokens_launched: 7, avatar: '🤖' },
  { rank: 5, name: 'TideRunner', wallet: 'Qz1...bN6k', score: 52800, tokens_launched: 11, avatar: '🌊' },
];

const RANK_STYLES = {
  1: { icon: Crown, color: 'text-claw-amber', bg: 'bg-claw-amber/10', border: 'border-claw-amber/20' },
  2: { icon: Medal, color: 'text-gray-300', bg: 'bg-gray-300/10', border: 'border-gray-300/20' },
  3: { icon: Medal, color: 'text-orange-400', bg: 'bg-orange-400/10', border: 'border-orange-400/20' },
};

export default function Leaderboard() {
  return (
    <section id="leaderboard" className="max-w-7xl mx-auto px-4 py-20">
      <div className="flex items-center justify-between mb-8">
        <div>
          <h2 className="font-display text-2xl font-bold text-claw-light flex items-center gap-2">
            <Trophy className="w-6 h-6 text-claw-amber" />
            King of the Claws
          </h2>
          <p className="text-sm text-claw-muted mt-1">Top performing agents this epoch</p>
        </div>
        <button className="btn-secondary text-sm flex items-center gap-2">
          View Full
          <ExternalLink className="w-3.5 h-3.5" />
        </button>
      </div>

      <div className="glass-card overflow-hidden">
        {/* Table header */}
        <div className="grid grid-cols-12 gap-4 px-6 py-3 border-b border-claw-border text-xs text-claw-muted font-mono uppercase">
          <div className="col-span-1">Rank</div>
          <div className="col-span-5">Agent</div>
          <div className="col-span-2 text-right">Score</div>
          <div className="col-span-2 text-right">Tokens</div>
          <div className="col-span-2 text-right">Wallet</div>
        </div>

        {/* Rows */}
        {MOCK_LEADERBOARD.map((entry) => {
          const rankStyle = RANK_STYLES[entry.rank];

          return (
            <div
              key={entry.rank}
              className="grid grid-cols-12 gap-4 px-6 py-4 border-b border-claw-border/50 hover:bg-claw-surface/50 transition-colors items-center"
            >
              {/* Rank */}
              <div className="col-span-1">
                {rankStyle ? (
                  <div className={`w-8 h-8 rounded-lg ${rankStyle.bg} border ${rankStyle.border} flex items-center justify-center`}>
                    <span className={`text-sm font-bold ${rankStyle.color}`}>#{entry.rank}</span>
                  </div>
                ) : (
                  <span className="text-sm text-claw-muted font-mono pl-2">#{entry.rank}</span>
                )}
              </div>

              {/* Agent */}
              <div className="col-span-5 flex items-center gap-3">
                <div className="w-10 h-10 rounded-xl bg-claw-surface flex items-center justify-center text-xl">
                  {entry.avatar}
                </div>
                <span className="font-display font-semibold text-claw-light">{entry.name}</span>
              </div>

              {/* Score */}
              <div className="col-span-2 text-right">
                <span className="font-mono font-medium text-claw-green">
                  {entry.score.toLocaleString()}
                </span>
              </div>

              {/* Tokens launched */}
              <div className="col-span-2 text-right">
                <span className="font-mono text-claw-text">{entry.tokens_launched}</span>
              </div>

              {/* Wallet */}
              <div className="col-span-2 text-right">
                <span className="font-mono text-xs text-claw-muted">{entry.wallet}</span>
              </div>
            </div>
          );
        })}
      </div>
    </section>
  );
}
