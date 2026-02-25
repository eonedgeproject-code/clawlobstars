import React from 'react';
import { Github, Twitter, MessageCircle, Zap } from 'lucide-react';

// ============================================
// PRICE TICKER
// ============================================
const TICKER_DATA = [
  { symbol: '$LOBS', price: '$0.0042', change: '+12.5%', positive: true },
  { symbol: '$CPNK', price: '$0.0089', change: '-5.2%', positive: false },
  { symbol: '$SHEL', price: '$0.0156', change: '+34.8%', positive: true },
  { symbol: '$DEEP', price: '$0.0023', change: '+8.1%', positive: true },
  { symbol: '$TIDE', price: '$0.0067', change: '-2.1%', positive: false },
  { symbol: '$REEF', price: '$0.0198', change: '+45.2%', positive: true },
  { symbol: '$CLAW', price: '$0.0531', change: '+7.0%', positive: true },
];

export function Ticker() {
  const items = [...TICKER_DATA, ...TICKER_DATA]; // duplicate for seamless loop

  return (
    <div className="fixed top-16 left-0 right-0 z-40 bg-claw-dark/90 backdrop-blur-sm border-b border-claw-border overflow-hidden">
      <div className="animate-ticker flex items-center gap-8 py-2 whitespace-nowrap">
        {items.map((item, i) => (
          <div key={i} className="flex items-center gap-2 text-xs">
            <span className="font-mono font-medium text-claw-text">{item.symbol}</span>
            <span className="text-claw-muted">{item.price}</span>
            <span className={item.positive ? 'text-claw-green' : 'text-claw-red'}>
              {item.change}
            </span>
          </div>
        ))}
      </div>
    </div>
  );
}

// ============================================
// FOOTER
// ============================================
const FOOTER_LINKS = [
  { label: 'Docs', href: '#' },
  { label: 'API', href: '#' },
  { label: 'Terms', href: '#' },
  { label: 'Privacy', href: '#' },
];

const SOCIAL_LINKS = [
  { icon: Twitter, href: 'https://twitter.com/clawlobstars', label: 'Twitter' },
  { icon: MessageCircle, href: '#', label: 'Discord' },
  { icon: Github, href: '#', label: 'GitHub' },
];

export function Footer() {
  return (
    <footer className="border-t border-claw-border mt-20">
      <div className="max-w-7xl mx-auto px-4 py-12">
        <div className="flex flex-col md:flex-row items-center justify-between gap-6">
          {/* Logo */}
          <div className="flex items-center gap-2">
            <div className="w-8 h-8 rounded-lg bg-gradient-to-br from-claw-red to-claw-orange flex items-center justify-center">
              <Zap className="w-4 h-4 text-white" />
            </div>
            <span className="font-display font-bold text-claw-light">
              Claw<span className="gradient-text">Lobstars</span>
            </span>
          </div>

          {/* Links */}
          <div className="flex items-center gap-6">
            {FOOTER_LINKS.map(({ label, href }) => (
              <a key={label} href={href} className="text-sm text-claw-muted hover:text-claw-light transition-colors">
                {label}
              </a>
            ))}
          </div>

          {/* Socials */}
          <div className="flex items-center gap-3">
            {SOCIAL_LINKS.map(({ icon: Icon, href, label }) => (
              <a
                key={label}
                href={href}
                target="_blank"
                rel="noopener noreferrer"
                className="w-9 h-9 rounded-lg bg-claw-surface hover:bg-claw-card border border-claw-border flex items-center justify-center text-claw-muted hover:text-claw-light transition-all"
              >
                <Icon className="w-4 h-4" />
              </a>
            ))}
          </div>
        </div>

        <div className="mt-8 pt-6 border-t border-claw-border/50 text-center text-xs text-claw-muted">
          © 2026 ClawLobstars. All rights reserved. Built on Solana.
        </div>
      </div>
    </footer>
  );
}
