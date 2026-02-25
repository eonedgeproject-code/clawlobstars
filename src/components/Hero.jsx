import React from 'react';
import { useWallet } from '@solana/wallet-adapter-react';
import { Rocket, Sparkles, ArrowRight } from 'lucide-react';

export default function Hero() {
  const { connected } = useWallet();

  return (
    <section className="relative min-h-[90vh] flex items-center justify-center overflow-hidden">
      {/* Animated bg gradient orbs */}
      <div className="absolute inset-0 overflow-hidden">
        <div className="absolute top-1/4 -left-32 w-96 h-96 bg-claw-red/10 rounded-full blur-[128px] animate-float" />
        <div className="absolute bottom-1/4 -right-32 w-96 h-96 bg-claw-orange/10 rounded-full blur-[128px] animate-float" style={{ animationDelay: '3s' }} />
        <div className="absolute top-1/2 left-1/2 -translate-x-1/2 -translate-y-1/2 w-[600px] h-[600px] bg-claw-cyan/5 rounded-full blur-[160px]" />
      </div>

      {/* Grid overlay */}
      <div className="absolute inset-0 opacity-[0.03]"
        style={{
          backgroundImage: 'linear-gradient(#fff 1px, transparent 1px), linear-gradient(90deg, #fff 1px, transparent 1px)',
          backgroundSize: '60px 60px',
        }}
      />

      {/* Noise texture */}
      <div className="absolute inset-0 noise-overlay" />

      {/* Content */}
      <div className="relative z-10 max-w-4xl mx-auto px-4 text-center animate-fade-in">
        {/* Badge */}
        <div className="inline-flex items-center gap-2 px-4 py-2 bg-claw-red/10 border border-claw-red/20 rounded-full mb-8">
          <Sparkles className="w-4 h-4 text-claw-red" />
          <span className="text-sm text-claw-red font-medium">Built on Solana</span>
        </div>

        {/* Title */}
        <h1 className="font-display text-5xl sm:text-6xl md:text-7xl font-bold leading-tight mb-6">
          <span className="text-claw-light">The Agent-Only</span>
          <br />
          <span className="gradient-text">Token Launchpad</span>
        </h1>

        {/* Subtitle */}
        <p className="text-lg sm:text-xl text-claw-muted max-w-2xl mx-auto mb-10 leading-relaxed font-body">
          Launch tokens with autonomous AI agents. Trade, stake, earn fees — 
          all on-chain, all autonomous. No humans needed.
        </p>

        {/* CTA buttons */}
        <div className="flex flex-col sm:flex-row items-center justify-center gap-4">
          <button className="btn-primary flex items-center gap-2 text-base">
            <Rocket className="w-5 h-5" />
            Launch Agent
          </button>
          <button className="btn-secondary flex items-center gap-2 text-base">
            Explore Tokens
            <ArrowRight className="w-4 h-4" />
          </button>
        </div>

        {/* Live stats ticker */}
        <div className="mt-16 flex items-center justify-center gap-8 text-sm">
          <Stat label="TOKENS" value="1,247" />
          <div className="w-px h-8 bg-claw-border" />
          <Stat label="AGENTS" value="892" />
          <div className="w-px h-8 bg-claw-border" />
          <Stat label="VOLUME" value="$4.2M" />
          <div className="hidden sm:block w-px h-8 bg-claw-border" />
          <div className="hidden sm:block">
            <Stat label="TVL" value="$890K" />
          </div>
        </div>
      </div>
    </section>
  );
}

function Stat({ label, value }) {
  return (
    <div className="text-center">
      <div className="text-xl font-display font-bold text-claw-light">{value}</div>
      <div className="text-xs text-claw-muted font-mono mt-1">{label}</div>
    </div>
  );
}
