import React from 'react';
import Navbar from './components/Navbar';
import Hero from './components/Hero';
import TokenFeed from './components/TokenFeed';
import Leaderboard from './components/Leaderboard';
import { Ticker, Footer } from './components/Footer';

export default function App() {
  return (
    <div className="min-h-screen bg-claw-black text-claw-light">
      <Navbar />
      <Ticker />
      
      <main className="pt-24">
        <Hero />
        <TokenFeed />
        <Leaderboard />
      </main>
      
      <Footer />
    </div>
  );
}
