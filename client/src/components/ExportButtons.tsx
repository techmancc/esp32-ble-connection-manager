import { Button } from "@/components/ui/button";
import {
  DropdownMenu,
  DropdownMenuContent,
  DropdownMenuItem,
  DropdownMenuTrigger,
} from "@/components/ui/dropdown-menu";
import { Download, FileJson, FileText } from "lucide-react";
import type { DashboardState, ParameterHistory } from "@shared/schema";
import { exportToJSON, exportToCSV } from "@/lib/export";

interface ExportButtonsProps {
  state: DashboardState;
  history: ParameterHistory[];
}

export function ExportButtons({ state, history }: ExportButtonsProps) {
  const handleExportJSON = () => {
    exportToJSON(state, history);
  };

  const handleExportCSV = () => {
    exportToCSV(history);
  };

  return (
    <DropdownMenu>
      <DropdownMenuTrigger asChild>
        <Button variant="outline" data-testid="button-export">
          <Download className="h-4 w-4 mr-2" />
          Export
        </Button>
      </DropdownMenuTrigger>
      <DropdownMenuContent align="end">
        <DropdownMenuItem onClick={handleExportJSON} data-testid="menu-export-json">
          <FileJson className="h-4 w-4 mr-2" />
          Export as JSON
        </DropdownMenuItem>
        <DropdownMenuItem onClick={handleExportCSV} data-testid="menu-export-csv">
          <FileText className="h-4 w-4 mr-2" />
          Export History as CSV
        </DropdownMenuItem>
      </DropdownMenuContent>
    </DropdownMenu>
  );
}
