import type { ConnectionParameterSet, ESP32Status, DashboardState, UpdateParameters, ConnectionParameters } from "@shared/schema";

export interface IStorage {
  getState(): Promise<DashboardState>;
  updateNextParameters(params: Partial<UpdateParameters>): Promise<DashboardState>;
  applyNextParameters(): Promise<DashboardState>;
  updateStatus(status: Partial<ESP32Status>): Promise<DashboardState>;
}

export class MemStorage implements IStorage {
  private state: DashboardState;

  constructor() {
    this.state = {
      parameters: {
        previous: null,
        current: {
          connectionIntervalMin: 280,
          connectionIntervalMax: 350,
          peripheralLatency: 8,
          supervisionTimeout: 3000,
        },
        next: null,
      },
      status: {
        isAdvertising: true,
        isConnected: false,
        connectedDeviceName: null,
        browserConnected: false,
      },
    };
  }

  async getState(): Promise<DashboardState> {
    return JSON.parse(JSON.stringify(this.state));
  }

  async updateNextParameters(params: Partial<UpdateParameters>): Promise<DashboardState> {
    const current = this.state.parameters.current;
    const existingNext = this.state.parameters.next || { ...current };
    
    this.state.parameters.next = {
      connectionIntervalMin: params.connectionIntervalMin ?? existingNext.connectionIntervalMin,
      connectionIntervalMax: params.connectionIntervalMax ?? existingNext.connectionIntervalMax,
      peripheralLatency: params.peripheralLatency ?? existingNext.peripheralLatency,
      supervisionTimeout: params.supervisionTimeout ?? existingNext.supervisionTimeout,
    };
    
    return this.getState();
  }

  async applyNextParameters(): Promise<DashboardState> {
    if (this.state.parameters.next) {
      this.state.parameters.previous = { ...this.state.parameters.current };
      this.state.parameters.current = { ...this.state.parameters.next };
      this.state.parameters.next = null;
    }
    
    return this.getState();
  }

  async updateStatus(status: Partial<ESP32Status>): Promise<DashboardState> {
    this.state.status = {
      ...this.state.status,
      ...status,
    };
    
    return this.getState();
  }
}

export const storage = new MemStorage();
