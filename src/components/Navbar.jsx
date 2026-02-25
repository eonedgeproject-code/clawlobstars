import React, { useState } from 'react';
import { useWallet } from '@solana/wallet-adapter-react';
import { WalletMultiButton } from '@solana/wallet-adapter-react-ui';
import { Menu, X, Zap, BarChart3, Rocket, Trophy } from 'lucide-react';

const NAV_ITEMS = [
  { label: 'Launch', icon: Rocket, href: '#launch' },
  { label: 'Trade', icon: BarChart3, href: '#trade' },
  { label: 'Leaderboard', icon: Trophy, href: '#leaderboard' },
];

export default function Navbar() {
  const { connected } = useWallet();
  const [mobileOpen, setMobileOpen] = useState(false);

  return (
    <nav className="fixed top-0 left-0 right-0 z-50 bg-claw-black/80 backdrop-blur-xl border-b border-claw-border">
      <div className="max-w-7xl mx-auto px-4 sm:px-6">
        <div className="flex items-center justify-between h-16">
          {/* Logo */}
          <a href="/" className="flex items-center gap-2 group">
            <div className="w-9 h-9 rounded-xl bg-gradient-to-br from-claw-red to-claw-orange flex items-center justify-center group-hover:scale-110 transition-transform">
              <Zap className="w-5 h-5 text-white" />
            </div>
            <span className="font-display font-bold text-lg text-claw-light">
              Claw<span className="gradient-text">Lobstars</span>
            </span>
          </a>

          {/* Desktop Nav */}
          <div className="hidden md:flex items-center gap-1">
            {NAV_ITEMS.map(({ label, icon: Icon, href }) => (
              <a
                key={label}
                href={href}
                className="flex items-center gap-2 px-4 py-2 text-sm text-claw-muted hover:text-claw-light hover:bg-claw-surface rounded-lg transition-colors"
              >
                <Icon className="w-4 h-4" />
                {label}
              </a>
            ))}
          </div>

          {/* Right side */}
          <div className="flex items-center gap-3">
            {connected && (
              <div className="hidden sm:flex items-center gap-1.5 px-3 py-1.5 bg-claw-green/10 border border-claw-green/20 rounded-full">
                <div className="w-1.5 h-1.5 bg-claw-green rounded-full animate-pulse-glow" />
                <span className="text-xs text-claw-green font-medium">Connected</span>
              </div>
            )}
            
            <WalletMultiButton className="!bg-claw-red !h-10 !rounded-xl !font-body !text-sm hover:!bg-claw-red/80 !transition-all" />

            {/* Mobile menu button */}
            <button
              onClick={() => setMobileOpen(!mobileOpen)}
              className="md:hidden p-2 text-claw-muted hover:text-claw-light"
            >
              {mobileOpen ? <X className="w-5 h-5" /> : <Menu className="w-5 h-5" />}
            </button>
          </div>
        </div>

        {/* Mobile Nav */}
        {mobileOpen && (
          <div className="md:hidden pb-4 animate-slide-up">
            {NAV_ITEMS.map(({ label, icon: Icon, href }) => (
              <a
                key={label}
                href={href}
                onClick={() => setMobileOpen(false)}
                className="flex items-center gap-3 px-4 py-3 text-claw-muted hover:text-claw-light hover:bg-claw-surface rounded-lg transition-colors"
              >
                <Icon className="w-4 h-4" />
                {label}
              </a>
            ))}
          </div>
        )}
      </div>
    </nav>
  );
}
