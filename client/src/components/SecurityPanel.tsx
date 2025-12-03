import { useState, useEffect } from "react";
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Badge } from "@/components/ui/badge";
import { Alert, AlertDescription } from "@/components/ui/alert";
import { Separator } from "@/components/ui/separator";
import { Switch } from "@/components/ui/switch";
import { Label } from "@/components/ui/label";
import { useToast } from "@/hooks/use-toast";
import { discoverEsp32 } from "@/lib/utils";
import { 
  Shield, 
  ShieldCheck, 
  ShieldX, 
  Bluetooth, 
  Key, 
  Users, 
  Trash2,
  Info,
  Lock,
  Unlock,
  AlertTriangle
} from "lucide-react";

interface PairedDevice {
  address: string;
  name?: string;
  pairedAt: number;
}

interface SecurityStatus {
  isConnected: boolean;
  isAuthenticated: boolean;
  currentDevice?: string;
  pairedDeviceCount: number;
  authRequired: boolean;
  pairingInProgress: boolean;
  currentPin?: string;
}

interface SecurityPanelProps {
  ws?: WebSocket | null;
}

export function SecurityPanel({ ws }: SecurityPanelProps) {
  const { toast } = useToast();
  const [securityStatus, setSecurityStatus] = useState<SecurityStatus>({
    isConnected: false,
    isAuthenticated: false,
    pairedDeviceCount: 0,
    authRequired: false,
    pairingInProgress: false,
  });
  
  const [pairedDevices, setPairedDevices] = useState<PairedDevice[]>([]);
  const [isLoading, setIsLoading] = useState(false);

  // Fetch security status from ESP32
  const fetchSecurityData = async () => {
    try {
      setIsLoading(true);
      const baseUrl = await discoverEsp32();
      const response = await fetch(`${baseUrl}/api/security`);
      
      if (!response.ok) {
        throw new Error('Failed to fetch security status');
      }
      
      const data = await response.json();
      setSecurityStatus(data.status || {});
      setPairedDevices(data.pairedDevices || []);
    } catch (error) {
      console.error('Error fetching security status:', error);
      toast({
        title: "Security Status Error",
        description: "Failed to fetch security information from ESP32",
        variant: "destructive",
      });
    } finally {
      setIsLoading(false);
    }
  };

  // Listen for real-time security updates via WebSocket
  useEffect(() => {
    if (!ws) return;

    const handleMessage = (event: MessageEvent) => {
      try {
        const data = JSON.parse(event.data);
        
        if (data.type === 'security_update') {
          setSecurityStatus(data.status || {});
          setPairedDevices(data.pairedDevices || []);
        } else if (data.type === 'pairing_started') {
          setSecurityStatus(prev => ({
            ...prev,
            pairingInProgress: true,
            currentPin: data.pin
          }));
          
          toast({
            title: "Pairing Started",
            description: `Enter PIN: ${data.pin} on your device`,
          });
        } else if (data.type === 'pairing_completed') {
          setSecurityStatus(prev => ({
            ...prev,
            pairingInProgress: false,
            currentPin: undefined
          }));
          
          fetchSecurityData(); // Refresh the status
          
          toast({
            title: "Pairing Successful",
            description: `Device ${data.deviceName || 'Unknown'} is now paired and authenticated`,
          });
        } else if (data.type === 'pairing_failed') {
          setSecurityStatus(prev => ({
            ...prev,
            pairingInProgress: false,
            currentPin: undefined
          }));
          
          toast({
            title: "Pairing Failed",
            description: data.reason || "Pairing was unsuccessful",
            variant: "destructive",
          });
        }
      } catch (error) {
        console.error('Error parsing security WebSocket message:', error);
      }
    };

    ws.addEventListener('message', handleMessage);
    return () => ws.removeEventListener('message', handleMessage);
  }, [ws, toast]);

  // Fetch initial status on component mount
  useEffect(() => {
    fetchSecurityData();
  }, []);

  // Remove a paired device
  const handleRemoveDevice = async (address: string) => {
    try {
      setIsLoading(true);
      const baseUrl = await discoverEsp32();
      const response = await fetch(`${baseUrl}/api/security/remove-device`, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({ address }),
      });

      if (!response.ok) {
        throw new Error('Failed to remove device');
      }

      await fetchSecurityData(); // Refresh the list
      
      toast({
        title: "Device Removed",
        description: "Device has been unpaired successfully",
      });
    } catch (error) {
      console.error('Error removing device:', error);
      toast({
        title: "Error",
        description: "Failed to remove device",
        variant: "destructive",
      });
    } finally {
      setIsLoading(false);
    }
  };

  // Toggle authentication requirement
  const handleToggleAuth = async (enabled: boolean) => {
    try {
      setIsLoading(true);
      const baseUrl = await discoverEsp32();
      const response = await fetch(`${baseUrl}/api/security/toggle-auth`, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({ enabled }),
      });

      if (!response.ok) {
        throw new Error('Failed to toggle authentication');
      }

      await fetchSecurityData(); // Refresh the status
      
      toast({
        title: "Authentication Settings Updated",
        description: `Authentication requirement ${enabled ? 'enabled' : 'disabled'}`,
      });
    } catch (error) {
      console.error('Error toggling authentication:', error);
      toast({
        title: "Error",
        description: "Failed to update authentication settings",
        variant: "destructive",
      });
    } finally {
      setIsLoading(false);
    }
  };

  const getConnectionIcon = () => {
    if (!securityStatus.isConnected) {
      return <ShieldX className="h-4 w-4 text-muted-foreground" />;
    }
    if (securityStatus.isAuthenticated) {
      return <ShieldCheck className="h-4 w-4 text-green-500" />;
    }
    return <Shield className="h-4 w-4 text-yellow-500" />;
  };

  const getConnectionStatus = () => {
    if (!securityStatus.isConnected) {
      return { text: "Disconnected", variant: "secondary" as const };
    }
    if (securityStatus.isAuthenticated) {
      return { text: "Authenticated", variant: "default" as const };
    }
    return { text: "Connected (Unauthenticated)", variant: "destructive" as const };
  };

  const connectionStatus = getConnectionStatus();

  return (
    <div className="space-y-6">
      {/* Security Status Card */}
      <Card>
        <CardHeader className="pb-3">
          <CardTitle className="flex items-center gap-2">
            <Bluetooth className="h-5 w-5" />
            Bluetooth Security
          </CardTitle>
        </CardHeader>
        <CardContent className="space-y-4">
          {/* Connection Status */}
          <div className="flex items-center justify-between">
            <div className="flex items-center gap-2">
              {getConnectionIcon()}
              <span className="text-sm font-medium">Connection Status</span>
            </div>
            <Badge variant={connectionStatus.variant}>
              {connectionStatus.text}
            </Badge>
          </div>

          {/* Current Device */}
          {securityStatus.currentDevice && (
            <div className="flex items-center justify-between">
              <div className="flex items-center gap-2">
                <Users className="h-4 w-4 text-muted-foreground" />
                <span className="text-sm font-medium">Current Device</span>
              </div>
              <span className="text-sm text-muted-foreground font-mono">
                {securityStatus.currentDevice}
              </span>
            </div>
          )}

          {/* Paired Devices Count */}
          <div className="flex items-center justify-between">
            <div className="flex items-center gap-2">
              <Key className="h-4 w-4 text-muted-foreground" />
              <span className="text-sm font-medium">Paired Devices</span>
            </div>
            <Badge variant="outline">
              {securityStatus.pairedDeviceCount}/10
            </Badge>
          </div>

          <Separator />

          {/* Authentication Requirement Toggle */}
          <div className="flex items-center justify-between">
            <div className="flex items-center gap-2">
              {securityStatus.authRequired ? (
                <Lock className="h-4 w-4 text-green-500" />
              ) : (
                <Unlock className="h-4 w-4 text-muted-foreground" />
              )}
              <Label htmlFor="auth-required" className="text-sm font-medium cursor-pointer">
                Require Authentication
              </Label>
            </div>
            <Switch
              id="auth-required"
              checked={securityStatus.authRequired}
              onCheckedChange={handleToggleAuth}
              disabled={isLoading}
            />
          </div>

          {securityStatus.authRequired && (
            <Alert>
              <Info className="h-4 w-4" />
              <AlertDescription className="text-xs">
                When enabled, only paired and authenticated devices can modify connection parameters.
              </AlertDescription>
            </Alert>
          )}
        </CardContent>
      </Card>

      {/* Pairing in Progress */}
      {securityStatus.pairingInProgress && (
        <Card className="border-blue-200 bg-blue-50 dark:border-blue-800 dark:bg-blue-950">
          <CardHeader className="pb-3">
            <CardTitle className="flex items-center gap-2 text-blue-700 dark:text-blue-300">
              <Key className="h-5 w-5" />
              Pairing in Progress
            </CardTitle>
          </CardHeader>
          <CardContent>
            {securityStatus.currentPin && (
              <div className="text-center space-y-2">
                <p className="text-sm text-blue-600 dark:text-blue-400">
                  Enter this PIN on your device:
                </p>
                <div className="text-3xl font-mono font-bold text-blue-700 dark:text-blue-300 bg-blue-100 dark:bg-blue-900 rounded-lg p-4">
                  {securityStatus.currentPin}
                </div>
                <p className="text-xs text-blue-500 dark:text-blue-400">
                  PIN expires in 30 seconds
                </p>
              </div>
            )}
          </CardContent>
        </Card>
      )}

      {/* Paired Devices List */}
      {pairedDevices.length > 0 && (
        <Card>
          <CardHeader className="pb-3">
            <CardTitle className="flex items-center gap-2">
              <Users className="h-5 w-5" />
              Paired Devices
            </CardTitle>
          </CardHeader>
          <CardContent>
            <div className="space-y-3">
              {pairedDevices.map((device, index) => (
                <div key={device.address} className="flex items-center justify-between p-3 border rounded-lg">
                  <div className="space-y-1">
                    <div className="flex items-center gap-2">
                      <span className="font-medium text-sm">
                        {device.name || 'Unknown Device'}
                      </span>
                      {securityStatus.currentDevice === device.address && (
                        <Badge variant="default" className="text-xs">Active</Badge>
                      )}
                    </div>
                    <div className="text-xs text-muted-foreground font-mono">
                      {device.address}
                    </div>
                    <div className="text-xs text-muted-foreground">
                      Paired: {new Date(device.pairedAt).toLocaleDateString()}
                    </div>
                  </div>
                  <Button
                    variant="outline"
                    size="sm"
                    onClick={() => handleRemoveDevice(device.address)}
                    disabled={isLoading}
                    className="text-destructive hover:text-destructive"
                  >
                    <Trash2 className="h-4 w-4" />
                  </Button>
                </div>
              ))}
            </div>
          </CardContent>
        </Card>
      )}

      {/* Security Information */}
      <Card className="border-amber-200 bg-amber-50 dark:border-amber-800 dark:bg-amber-950">
        <CardHeader className="pb-3">
          <CardTitle className="flex items-center gap-2 text-amber-700 dark:text-amber-300">
            <AlertTriangle className="h-5 w-5" />
            Security Information
          </CardTitle>
        </CardHeader>
        <CardContent className="space-y-2">
          <p className="text-xs text-amber-600 dark:text-amber-400">
            • This device uses BLE Secure Connections with MITM protection
          </p>
          <p className="text-xs text-amber-600 dark:text-amber-400">
            • Pairing requires PIN entry for authentication
          </p>
          <p className="text-xs text-amber-600 dark:text-amber-400">
            • Up to 10 devices can be paired simultaneously
          </p>
          <p className="text-xs text-amber-600 dark:text-amber-400">
            • Enable authentication requirement for enhanced security
          </p>
        </CardContent>
      </Card>
    </div>
  );
}