import { Component } from '@angular/core';

import { RgwDaemonService } from '../../../shared/api/rgw-daemon.service';
import { CdTableColumn } from '../../../shared/models/cd-table-column';
import { CdTableFetchDataContext } from '../../../shared/models/cd-table-fetch-data-context';
import { CdTableSelection } from '../../../shared/models/cd-table-selection';
import { Permission } from '../../../shared/models/permissions';
import { CephShortVersionPipe } from '../../../shared/pipes/ceph-short-version.pipe';
import { AuthStorageService } from '../../../shared/services/auth-storage.service';

@Component({
  selector: 'cd-rgw-daemon-list',
  templateUrl: './rgw-daemon-list.component.html',
  styleUrls: ['./rgw-daemon-list.component.scss']
})
export class RgwDaemonListComponent {
  columns: CdTableColumn[] = [];
  daemons: object[] = [];
  selection: CdTableSelection = new CdTableSelection();
  grafanaPermission: Permission;

  constructor(
    private rgwDaemonService: RgwDaemonService,
    private authStorageService: AuthStorageService,
    cephShortVersionPipe: CephShortVersionPipe
  ) {
    this.grafanaPermission = this.authStorageService.getPermissions().grafana;
    this.columns = [
      {
        name: 'ID',
        prop: 'id',
        flexGrow: 2
      },
      {
        name: 'Hostname',
        prop: 'server_hostname',
        flexGrow: 2
      },
      {
        name: 'Version',
        prop: 'version',
        flexGrow: 1,
        pipe: cephShortVersionPipe
      }
    ];
  }

  getDaemonList(context: CdTableFetchDataContext) {
    this.rgwDaemonService.list().subscribe(
      (resp: object[]) => {
        this.daemons = resp;
      },
      () => {
        context.error();
      }
    );
  }

  updateSelection(selection: CdTableSelection) {
    this.selection = selection;
  }
}
