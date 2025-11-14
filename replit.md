# ESP32 BLE Connection Parameter Dashboard

## Overview

This application is a web-based dashboard for managing and monitoring Bluetooth Low Energy (BLE) connection parameters for ESP32 GATT server devices. The system allows users to view current connection parameters, configure next parameters, and apply changes in real-time. The dashboard displays connection status information and provides a professional interface for adjusting critical BLE connection settings including connection intervals, peripheral latency, and supervision timeout.

## User Preferences

Preferred communication style: Simple, everyday language.

## System Architecture

### Frontend Architecture

**Framework**: React with TypeScript using Vite as the build tool and development server.

**UI Component Library**: shadcn/ui components built on Radix UI primitives, providing accessible and customizable UI components. The design follows Material Design 3 principles with a focus on clarity and professional presentation of technical data.

**Styling**: Tailwind CSS with a custom theme configuration supporting both light and dark modes. The theme uses CSS variables for colors, enabling dynamic theming. Typography is based on the Inter font family for consistency.

**State Management**: React hooks for local state management, with TanStack Query for server state synchronization and caching. Real-time updates are handled through WebSocket connections.

**Routing**: Wouter library for lightweight client-side routing.

**Key Design Decisions**:
- Component-based architecture with separation of concerns (StatusPanel, ConnectionParametersTable, ParameterInput)
- Real-time state synchronization using WebSocket for live updates from the ESP32 device
- Form validation using Zod schemas shared between client and server
- Responsive design with mobile-first approach using Tailwind's responsive utilities

### Backend Architecture

**Runtime**: Node.js with Express.js framework.

**Language**: TypeScript with ES modules.

**API Design**: RESTful API endpoints for CRUD operations on connection parameters:
- `GET /api/state` - Retrieve current dashboard state
- `POST /api/parameters/next` - Update next parameter values
- `POST /api/parameters/apply` - Apply next parameters as current

**Real-time Communication**: WebSocket server using the `ws` library for bidirectional communication. Broadcasts state updates to all connected clients when parameters change.

**Data Storage**: In-memory storage implementation (`MemStorage`) that maintains three sets of connection parameters (previous, current, next). The storage interface (`IStorage`) is abstracted to allow future database implementations.

**Key Design Decisions**:
- Separation of storage logic through interface abstraction, allowing easy migration to persistent database
- WebSocket integration with HTTP server for real-time state synchronization
- Middleware for request logging and JSON body parsing
- Schema validation using Zod for type-safe parameter updates

### Data Storage Solutions

**Current Implementation**: In-memory storage using a TypeScript class (`MemStorage`) that maintains application state in RAM. State includes:
- Three parameter sets (previous, current, next)
- ESP32 status information (advertising state, connection state, connected device name, browser connection status)

**Database Configuration**: Drizzle ORM is configured for PostgreSQL with migration support, though not currently utilized. The configuration suggests planned future integration with persistent storage.

**Schema Definition**: Shared TypeScript schemas using Zod for runtime validation and type inference. Connection parameters include:
- Connection Interval Min/Max (7.5-4000 ms)
- Peripheral Latency (0-499 count)
- Supervision Timeout (100-32000 ms)

**Rationale**: In-memory storage provides fast access and simple implementation for the initial version. The abstracted storage interface enables future migration to PostgreSQL without changing business logic.

### External Dependencies

**UI Components & Styling**:
- `@radix-ui/*` - Accessible component primitives (dialogs, popovers, dropdowns, etc.)
- `tailwindcss` - Utility-first CSS framework
- `class-variance-authority` - Type-safe variant styling
- `lucide-react` - Icon library
- Google Fonts (Inter) - Typography via CDN

**State Management & Data Fetching**:
- `@tanstack/react-query` - Server state management and caching
- `react-hook-form` - Form state management
- `@hookform/resolvers` - Form validation integration
- `zod` - Schema validation and type inference

**Backend Framework & Utilities**:
- `express` - Web application framework
- `ws` - WebSocket server implementation
- `drizzle-orm` - SQL ORM with TypeScript support
- `@neondatabase/serverless` - Serverless PostgreSQL driver

**Development Tools**:
- `vite` - Build tool and development server
- `@vitejs/plugin-react` - React plugin for Vite
- `tsx` - TypeScript execution environment
- `esbuild` - JavaScript bundler for production builds
- `@replit/*` - Replit-specific development plugins (error overlay, cartographer, dev banner)

**Session Management**:
- `connect-pg-simple` - PostgreSQL session store (configured but not actively used with in-memory storage)

**Date/Time Handling**:
- `date-fns` - Modern JavaScript date utility library