import type { Metadata } from "next";
import "./globals.css";

export const metadata: Metadata = {
  title: "FactoryFlow | Operator HMI",
  description: "Live conveyor telemetry and operator commands",
};

export default function RootLayout({ children }: Readonly<{ children: React.ReactNode }>) {
  return <html lang="en"><body>{children}</body></html>;
}
