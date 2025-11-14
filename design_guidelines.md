# ESP32 BLE Connection Parameter Dashboard - Design Guidelines

## Design Approach

**Selected Framework:** Material Design 3  
**Rationale:** Data-heavy technical interface requiring clear information hierarchy, reliable component patterns, and strong visual feedback for state changes and user actions.

**Design Principles:**
1. **Clarity First** - Technical data must be immediately scannable and understandable
2. **Status Visibility** - Connection states and system status always visible
3. **Action Confidence** - Clear feedback for all user interactions
4. **Professional Restraint** - Clean, focused interface without unnecessary decoration

---

## Core Design Elements

### Typography
- **Primary Font:** Inter via Google Fonts CDN
- **Headings:** 
  - Dashboard Title: text-2xl (24px), font-semibold
  - Section Headers: text-lg (18px), font-medium
  - Parameter Labels: text-sm (14px), font-medium
- **Data Display:**
  - Parameter Values: text-base (16px), font-mono for numerical precision
  - Status Text: text-sm (14px), font-normal
  - Help Text: text-xs (12px), font-normal

### Layout System
**Spacing Primitives:** Tailwind units of 2, 4, 6, and 8  
- Component padding: p-4, p-6, p-8
- Element gaps: gap-4, gap-6
- Section margins: my-6, my-8
- Maximum container width: max-w-5xl (centered dashboard)

---

## Component Library

### Dashboard Structure
1. **Header Bar**
   - Dashboard title with ESP32 icon (Heroicons: cpu-chip)
   - Real-time connection status badge (connected/disconnected to ESP32)
   - Fixed position: sticky top-0, backdrop-blur with subtle background

2. **Status Panel** (Top Section)
   - Grid layout: 2x2 status cards on desktop, stack on mobile
   - Each card contains:
     - Icon indicator (Heroicons: signal, device-phone-mobile, wifi, check-circle)
     - Status label
     - Dynamic value/state with color coding
   - Status states:
     - BLE Advertising: Active (green) / Inactive (gray)
     - BLE Connected: Connected (green) / Disconnected (gray)
     - Connected Device: Device name or "None"
     - Browser Connection: Connected (green) / Disconnected (red)

3. **Connection Parameters Table**
   - Three-column layout (Previous | Current | Next)
   - Four parameter rows:
     - Connection Interval Min (ms)
     - Connection Interval Max (ms)
     - Peripheral Latency (count)
     - Supervision Timeout (ms)
   - Column treatments:
     - **Previous:** Read-only, muted text, lighter background
     - **Current:** Read-only, emphasized text, default background
     - **Next:** Editable input fields, highlighted border on focus
   - Table styling: bordered cells, alternating row backgrounds for readability
   - Mobile: Stack columns vertically with clear labels

4. **Action Controls** (Bottom Section)
   - Two primary buttons, horizontal layout:
     - **Send Next:** Primary button, full emphasis (filled, accent color)
     - **Clear Next:** Secondary button, outline style
   - Button sizing: py-3 px-6, text-base
   - Gap between buttons: gap-4
   - Disabled states when no changes pending

### Form Inputs (Next Column)
- Number inputs with step controls
- Bordered style with focus rings
- Validation indicators (check/error icons)
- Inline range hints below each input
- Padding: px-4 py-2

### Visual Feedback
- Loading spinner overlay when sending parameters
- Toast notifications for success/error messages (top-right positioning)
- Subtle pulse animation on status badges when values update
- Input validation: green border for valid, red for out-of-range values

---

## Iconography
**Library:** Heroicons (via CDN)  
**Icons Used:**
- cpu-chip (ESP32/header)
- signal (BLE advertising)
- device-phone-mobile (connected device)
- wifi (browser connection)
- check-circle (status indicators)
- arrow-up-tray (send action)
- x-mark (clear action)

---

## Accessibility
- High contrast text-to-background ratios (WCAG AA minimum)
- Focus indicators on all interactive elements (ring-2 ring-offset-2)
- Keyboard navigation support (Tab order: status cards → inputs → buttons)
- ARIA labels on status indicators and icon buttons
- Form input labels associated with inputs via id/for attributes
- Error messages linked to invalid inputs

---

## Responsive Behavior
- **Desktop (lg:):** Full dashboard layout, side-by-side table columns
- **Tablet (md:):** Status panel remains 2x2, table begins to condense
- **Mobile (base):** 
  - Stack status cards vertically
  - Table transforms to card-based layout per parameter
  - Full-width buttons, stacked vertically
  - Padding reduces to p-4 for screen edge spacing

---

## Animation Guidelines
**Minimal, Purposeful Only:**
- Smooth transitions on status changes (transition-colors duration-200)
- Input focus states (transition-all duration-150)
- Toast slide-in from top-right (translate-y animation)
- No decorative animations, scrolling effects, or micro-interactions