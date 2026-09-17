import type { NextConfig } from "next";

const nextConfig: NextConfig = {
  async rewrites() {
    // HTTP forwarding only; FastAPI remains the gateway to C++.
    const gateway = process.env.GATEWAY_URL ?? "http://127.0.0.1:8000";
    return ["status", "start", "stop", "reset"].map((endpoint) => ({
      source: `/api/${endpoint}`,
      destination: `${gateway}/api/${endpoint}`,
    }));
  },
};

export default nextConfig;
